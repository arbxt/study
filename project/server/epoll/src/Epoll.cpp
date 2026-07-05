#include "Epoll.h"
#include "handle.h"
#include "http.h"
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

static constexpr int kIdleTimeoutSeconds = 5;

Epoller::Epoller(Tcpserver &server)
    : server_(server), listen_fd_(server.listen_fd()), epoll_fd_(-1),
      events_(1024) {
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
    auto ret = connections_.emplace(client_fd, conn);
    if (!ret.second) {
      std::cerr << "duplicate connection fd = " << client_fd << std::endl;
      close(client_fd);
      continue;
    }

    add_fd(client_fd, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP);

    std::cout << "new client fd = " << client_fd << std::endl;
  }
}

void Epoller::handle_client_events(int fd, uint32_t events) {
  auto it = connections_.find(fd);
  if (it == connections_.end()) {
    del_fd(fd);
    return;
  }

  Connection &conn = it->second;

  if (events & EPOLLERR) {
    close_client(fd);
    return;
  }

  if (events & (EPOLLRDHUP | EPOLLHUP)) {
    conn.close_after_write = true;
    std::cout << "fd " << fd << " hup/rdhup, close_after_write=true"
              << std::endl;
  }

  if (events & EPOLLIN) {
    char buffer[4096];

    while (true) {
      ssize_t n = read(fd, buffer, sizeof(buffer));

      if (n > 0) {
        conn.read_buffer.append(buffer, static_cast<size_t>(n));
        conn.last_active = std::time(nullptr);
        continue;
      }

      if (n == 0) {
        conn.close_after_write = true;
        std::cout << "fd " << fd << " read EOF,close_after_writ=true"
                  << std::endl;
        break;
      }

      if (errno == EINTR) {
        continue;
      }

      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }

      close_client(fd);
      return;
    }
    process_http_buffer(fd, conn);
  }

  if (events & EPOLLOUT) {
    if (send_response(fd, "")) {
      return;
    }
  }

  if (conn.close_after_write) {
    if (conn.write_buffer.empty()) {
      close_client(fd);
      return;
    }

    enable_write(fd);
  }
}

void Epoller::check_timeout_connections() {
  time_t now = std::time(nullptr);

  std::vector<int> expired_fds;

  for (const auto &kv : connections_) {
    int fd = kv.first;
    const Connection &conn = kv.second;

    if (now - conn.last_active > kIdleTimeoutSeconds) {
      expired_fds.push_back(fd);
    }
  }

  for (int fd : expired_fds) {
    std::cout << "connection timeout, fd = " << fd << std::endl;
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

  del_fd(fd);

  if (close(fd) < 0) {
    std::cerr << "close client fd failed, fd = " << fd
              << ", error: " << std::strerror(errno) << std::endl;
  } else {
    std::cout << "client fd " << fd << " closed" << std::endl;
  }

  connections_.erase(fd);
}

void Epoller::process_http_buffer(int fd, Connection &conn) {
  while (true) {
    ParseResult result = try_parse_http_request(conn.read_buffer);

    if (result.status == ParseStatus::Incomplete) {
      break;
    }

    if (result.status == ParseStatus::Error) {
      std::string resp = make_http_response(400, "Bad Request\n", "text/plain");

      conn.close_after_write = true;

      if (send_response(fd, resp)) {
        return;
      }

      break;
    }

    std::string resp = handle_http_request(result.request);

    conn.read_buffer.erase(0, result.consumed);

    conn.close_after_write = true;

    if (send_response(fd, resp)) {
      return;
    }

    break;
  }
}

bool Epoller::send_response(int fd, const std::string &response) {
  auto it = connections_.find(fd);
  if (it == connections_.end()) {
    del_fd(fd);
    return true;
  }

  Connection &conn = it->second;

  if (!response.empty()) {
    conn.write_buffer.append(response);
  }

  if (conn.write_buffer.empty()) {
    disable_write(fd);

    if (conn.close_after_write) {
      close_client(fd);
      return true;
    }

    return false;
  }

  std::string &pending = conn.write_buffer;

  ssize_t n = shortWrite(fd, pending.data(), pending.size());

  if (n < 0) {
    close_client(fd);
    return true;
  }

  if (n > 0) {
    conn.last_active = std::time(nullptr);
  }

  size_t written = static_cast<size_t>(n);

  if (written == pending.size()) {
    pending.clear();
    disable_write(fd);

    if (conn.close_after_write) {
      close_client(fd);
      return true;
    }

    return false;
  }

  if (written > 0) {
    pending.erase(0, written);
  }

  enable_write(fd);
  return false;
}

void Epoller::enable_write(int fd) {
  mod_fd(fd, EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLRDHUP);
}

void Epoller::disable_write(int fd) {
  mod_fd(fd, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP);
}

void Epoller::add_fd(int fd, uint32_t events) {
  epoll_event ev{};
  ev.events = events;
  ev.data.fd = fd;

  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
    std::cerr << "epoll_ctl add failed,fd=" << fd << ",error"
              << std::strerror(errno) << std::endl;
  }
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