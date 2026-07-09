#include "Epoll.h"
#include "config.h"
#include "http.h"
#include "router.h"
#include "server.h"
#include "utils.h"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <sys/epoll.h>
#include <sys/types.h>
#include <unistd.h>

Epoller::Epoller(Tcpserver &server, const ServerConfig &config)
    : config_(config), server_(server), listen_fd_(server.listen_fd()),
      epoll_fd_(-1), events_(config_.max_events) {
  epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
  if (epoll_fd_ < 0) {
    std::cerr << "epoll create1 failed:" << std::strerror(errno) << std::endl;
    throw std::runtime_error("epoll create failed");
  }
  add_fd(listen_fd_, EPOLLIN | EPOLLERR | EPOLLHUP);
}

Epoller::~Epoller() {
  for (auto &kv : connections_) {
    int fd = kv.first;
    if (fd >= 0) {
      close(fd);
    }
  }

  connections_.clear();

  if (epoll_fd_ >= 0) {
    close(epoll_fd_);
    epoll_fd_ = -1;
  }
}

void Epoller::loop() {
  while (true) {
    int n = epoll_wait(epoll_fd_, events_.data(),
                       static_cast<int>(events_.size()), 1000);

    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      std::cerr << "epoll_wait failed" << std::strerror(errno) << std::endl;
      break;
    }

    for (int i = 0; i < n; ++i) {
      int fd = events_[i].data.fd;
      uint32_t ev = events_[i].events;

      if (fd == listen_fd_) {
        if (ev & (EPOLLERR | EPOLLHUP)) {
          std::cerr << "listen fd error, server stop" << std::endl;
          return;
        }

        if (ev & EPOLLIN) {
          handle_new_connection();
        }
        continue;
      }
      handle_client_events(fd, ev);
    }

    check_timeout_connections();
  }
}

void Epoller::handle_new_connection() {
  while (true) {
    int client_fd = server_.accept_client();

    if (client_fd < 0) {
      if (errno == EINTR) {
        continue;
      }

      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }

      std::cerr << "accept failed: " << std::strerror(errno) << std::endl;
      return;
    }

    if (!setFdnonBlocking(client_fd)) {
      close(client_fd);
      continue;
    }

    Connection conn;
    conn.last_active = std::time(nullptr);

    if (!add_fd(client_fd, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
      close(client_fd);
      continue;
    }

    auto ret = connections_.emplace(client_fd, std::move(conn));
    if (!ret.second) {
      del_fd(client_fd);
      close(client_fd);
      continue;
    }

    std::cout << "new client fd = " << client_fd << std::endl;
  }
}

void Epoller::handle_client_events(int fd, uint32_t events) {
  auto it = connections_.find(fd);
  if (it == connections_.end()) {
    std::cout << "[event] fd=" << fd
              << " no connection state, delete from epoll" << std::endl;
    del_fd(fd);
    return;
  }

  Connection &conn = it->second;

  std::cout << "[event] fd=" << fd << " events=" << events
            << " in=" << static_cast<bool>(events & EPOLLIN)
            << " out=" << static_cast<bool>(events & EPOLLOUT)
            << " err=" << static_cast<bool>(events & EPOLLERR)
            << " hup=" << static_cast<bool>(events & EPOLLHUP)
            << " rdhup=" << static_cast<bool>(events & EPOLLRDHUP)
            << " read_buffer=" << conn.read_buffer.size()
            << " write_buffer=" << conn.write_buffer.size()
            << " close_after_write=" << conn.close_after_write << std::endl;

  if (events & EPOLLERR) {
    std::cout << "[event] fd=" << fd << " EPOLLERR, close immediately"
              << std::endl;
    close_client(fd);
    return;
  }

  if (events & (EPOLLRDHUP | EPOLLHUP)) {
    conn.close_after_write = true;
    std::cout << "[event] fd=" << fd << " hup/rdhup, close_after_write=true"
              << std::endl;
  }

  if (events & EPOLLIN) {
    char buffer[4096];

    while (true) {
      ssize_t n = read(fd, buffer, sizeof(buffer));

      if (n > 0) {
        conn.read_buffer.append(buffer, static_cast<size_t>(n));
        conn.last_active = std::time(nullptr);

        std::cout << "[read] fd=" << fd << " n=" << n
                  << " read_buffer_size=" << conn.read_buffer.size()
                  << std::endl;

        continue;
      }

      if (n == 0) {
        conn.close_after_write = true;
        std::cout << "[read EOF] fd=" << fd
                  << " read_buffer_size=" << conn.read_buffer.size()
                  << " write_buffer_size=" << conn.write_buffer.size()
                  << " close_after_write=true" << std::endl;
        break;
      }

      if (errno == EINTR) {
        std::cout << "[read] fd=" << fd << " interrupted by signal, retry"
                  << std::endl;
        continue;
      }

      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        std::cout << "[read done] fd=" << fd
                  << " read_buffer_size=" << conn.read_buffer.size()
                  << std::endl;
        break;
      }

      std::cerr << "[read error] fd=" << fd << " error=" << std::strerror(errno)
                << std::endl;
      close_client(fd);
      return;
    }

    if (process_http_buffer(fd, conn)) {
      return;
    }
  }

  if (events & EPOLLOUT) {
    std::cout << "[event] fd=" << fd << " EPOLLOUT, try flush write_buffer="
              << conn.write_buffer.size() << std::endl;
    if (send_response(fd, "")) {
      return;
    }
  }

  if (conn.close_after_write) {
    if (conn.write_buffer.empty()) {
      std::cout << "[close after write] fd=" << fd
                << " write_buffer empty, close now" << std::endl;
      close_client(fd);
      return;
    }

    std::cout << "[close after write] fd=" << fd
              << " pending=" << conn.write_buffer.size() << ", enable EPOLLOUT"
              << std::endl;
    enable_write(fd);
  }
}

void Epoller::check_timeout_connections() {
  time_t now = std::time(nullptr);

  std::vector<int> expired_fds;

  for (const auto &kv : connections_) {
    int fd = kv.first;
    const Connection &conn = kv.second;

    if (now - conn.last_active > config_.idle_timeout_seconds) {
      expired_fds.push_back(fd);
    }
  }

  for (int fd : expired_fds) {
    auto it = connections_.find(fd);
    if (it != connections_.end()) {
      std::cout << "[timeout] fd=" << fd
                << " idle_seconds=" << (now - it->second.last_active)
                << " read_buffer=" << it->second.read_buffer.size()
                << " write_buffer=" << it->second.write_buffer.size()
                << " close_after_write=" << it->second.close_after_write
                << std::endl;
    } else {
      std::cout << "[timeout] fd=" << fd << " no connection state" << std::endl;
    }

    close_client(fd);
  }
}

void Epoller::close_client(int fd) {
  if (fd < 0) {
    return;
  }

  if (fd == listen_fd_) {
    std::cerr << "close_client should not close listen_fd, fd = " << fd
              << std::endl;
    return;
  }

  auto it = connections_.find(fd);
  if (it != connections_.end()) {
    std::cout << "[close_client] fd=" << fd
              << " read_buffer=" << it->second.read_buffer.size()
              << " write_buffer=" << it->second.write_buffer.size()
              << " close_after_write=" << it->second.close_after_write
              << std::endl;
  } else {
    std::cout << "[close_client] fd=" << fd << " no connection state"
              << std::endl;
  }

  del_fd(fd);

  if (close(fd) < 0) {
    std::cerr << "close client fd failed, fd = " << fd
              << ", error: " << std::strerror(errno) << std::endl;
  } else {
    std::cout << "[close_client] fd=" << fd << " closed" << std::endl;
  }

  connections_.erase(fd);
}

bool Epoller::process_http_buffer(int fd, Connection &conn) {
  int processed = 0;

  while (processed < config_.max_requests_per_event) {
    std::cout << "[process begin] fd=" << fd
              << " read_buffer_size=" << conn.read_buffer.size()
              << " processed=" << processed << std::endl;

    ParseResult result =
        try_parse_http_request(conn.read_buffer, config_.max_request_head_size,
                               config_.max_request_body_size);

    if (result.status == ParseStatus::Incomplete) {
      std::cout << "[parse incomplete] fd=" << fd
                << " read_buffer_size=" << conn.read_buffer.size() << std::endl;
      return false;
    }

    if (result.status == ParseStatus::Error) {
      std::cout << "[parse error] fd=" << fd
                << " read_buffer_size=" << conn.read_buffer.size()
                << " preview=[" << conn.read_buffer.substr(0, 120) << "]"
                << std::endl;

      std::string resp =
          make_http_response(400, "Bad Request\n", "text/plain", false);

      conn.close_after_write = true;

      if (send_response(fd, resp)) {
        return true;
      }

      return false;
    }

    if (result.consumed == 0 || result.consumed > conn.read_buffer.size()) {
      std::cout << "[parse invalid consumed] fd=" << fd
                << " consumed=" << result.consumed
                << " read_buffer_size=" << conn.read_buffer.size() << std::endl;

      std::string resp =
          make_http_response(400, "Bad Request\n", "text/plain", false);

      conn.close_after_write = true;

      if (send_response(fd, resp)) {
        return true;
      }

      return false;
    }

    bool keep_alive = should_keep_alive(result.request);
    std::string resp = handle_http_request(result.request, keep_alive);

    std::cout << "[parse complete] fd=" << fd
              << " method=" << result.request.method
              << " path=" << result.request.path
              << " body_size=" << result.request.body.size()
              << " consumed=" << result.consumed << " keep_alive=" << keep_alive
              << " read_buffer_before=" << conn.read_buffer.size() << std::endl;

    conn.read_buffer.erase(0, result.consumed);

    std::cout << "[after erase] fd=" << fd
              << " read_buffer_after=" << conn.read_buffer.size() << std::endl;

    if (!keep_alive) {
      conn.close_after_write = true;
      std::cout << "[http] fd=" << fd
                << " keep_alive=false, close_after_write=true" << std::endl;
    }

    if (send_response(fd, resp)) {
      std::cout << "[http] fd=" << fd << " send_response asked caller to stop"
                << std::endl;
      return true;
    }

    ++processed;

    if (!keep_alive) {
      return false;
    }
  }

  std::cout << "[process limit reached] fd=" << fd << " processed=" << processed
            << " remain=" << conn.read_buffer.size() << std::endl;

  return false;
}

bool Epoller::send_response(int fd, const std::string &response) {
  auto it = connections_.find(fd);
  if (it == connections_.end()) {
    std::cout << "[send_response] fd=" << fd
              << " no connection state, delete from epoll" << std::endl;
    del_fd(fd);
    return true;
  }

  Connection &conn = it->second;

  if (!response.empty()) {
    std::cout << "[send_response] fd=" << fd << " append=" << response.size()
              << " write_buffer_before=" << conn.write_buffer.size()
              << std::endl;
    conn.write_buffer.append(response);
  }

  if (conn.write_buffer.empty()) {
    std::cout << "[send_response] fd=" << fd << " write_buffer empty"
              << std::endl;
    disable_write(fd);

    if (conn.close_after_write) {
      std::cout << "[send_response] fd=" << fd
                << " close_after_write=true and nothing to write" << std::endl;
      close_client(fd);
      return true;
    }

    return false;
  }

  std::string &pending = conn.write_buffer;
  size_t pending_before = pending.size();

  ssize_t n = shortWrite(fd, pending.data(), pending.size());

  if (n < 0) {
    std::cerr << "[write error] fd=" << fd << " pending=" << pending_before
              << " error=" << std::strerror(errno) << std::endl;
    close_client(fd);
    return true;
  }

  if (n > 0) {
    conn.last_active = std::time(nullptr);
  }

  size_t written = static_cast<size_t>(n);

  std::cout << "[write] fd=" << fd << " pending_before=" << pending_before
            << " written=" << written << std::endl;

  if (written == pending.size()) {
    pending.clear();
    disable_write(fd);

    std::cout << "[write complete] fd=" << fd
              << " close_after_write=" << conn.close_after_write << std::endl;

    if (conn.close_after_write) {
      close_client(fd);
      return true;
    }

    return false;
  }

  if (written > 0) {
    pending.erase(0, written);
  }

  std::cout << "[write partial] fd=" << fd << " remain=" << pending.size()
            << ", enable EPOLLOUT" << std::endl;

  enable_write(fd);
  return false;
}

void Epoller::enable_write(int fd) {
  mod_fd(fd, EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLRDHUP);
}

void Epoller::disable_write(int fd) {
  mod_fd(fd, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP);
}

bool Epoller::add_fd(int fd, uint32_t events) {
  epoll_event ev{};
  ev.events = events;
  ev.data.fd = fd;

  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
    std::cerr << "epoll_ctl ADD failed, fd = " << fd
              << ", error: " << std::strerror(errno) << std::endl;
    return false;
  }

  return true;
}

void Epoller::mod_fd(int fd, uint32_t events) {
  epoll_event ev{};
  ev.events = events;
  ev.data.fd = fd;

  if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) < 0) {
    std::cerr << "epoll_ctl MOD failed, fd = " << fd
              << ", error: " << std::strerror(errno) << std::endl;
  }
}

void Epoller::del_fd(int fd) {
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
    if (errno != ENOENT && errno != EBADF) {
      std::cerr << "epoll_ctl DEL failed, fd = " << fd
                << ", error: " << std::strerror(errno) << std::endl;
    }
  }
}