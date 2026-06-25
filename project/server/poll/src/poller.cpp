#include "poller.h"
#include "handle.h"
#include "server.h"
#include "utils.h"

#include <cerrno>
#include <cstddef>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/poll.h>
#include <sys/types.h>
#include <unistd.h>

Poller::Poller(TcpServer &server)
    : server_(server), listen_fd_(server.listen_fd()) {
  poll_fds_.push_back({listen_fd_, POLLIN, 0});
}

Poller::~Poller() {
  for (size_t i = 0; i < poll_fds_.size(); ++i) {
    if (poll_fds_[i].fd == listen_fd_) {
      continue;
    }
    if (poll_fds_[i].fd >= 0) {
      close(poll_fds_[i].fd);
      poll_fds_[i].fd = -1;
    }
  }
  write_buffers_.clear();
}

void Poller::loop() {

  while (true) {
    int ready_count = poll(poll_fds_.data(), poll_fds_.size(), -1);

    if (ready_count < 0) {
      if (errno == EINTR) {
        continue;
      }
      std::cerr << "poller failed" << std::strerror(errno) << std::endl;
      break;
    }

    if (ready_count == 0) {
      continue;
    }

    for (size_t i = 0; i < poll_fds_.size() && ready_count > 0;) {
      if (poll_fds_[i].revents == 0) {
        ++i;
        continue;
      }

      --ready_count;

      if (poll_fds_[i].fd == listen_fd_) {
        if (poll_fds_[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
          std::cerr << "listen fd error, server will stop" << std::endl;
          return;
        }

        if (poll_fds_[i].revents & POLLIN) {
          handle_new_connection();
        }

        ++i;
        continue;
      }
      bool removed = handle_client_event(i);

      // erase后vector前进一位，--i防止跳过元素
      if (!removed) {
        ++i;
      }
    }
  }
}

void Poller::handle_new_connection() {
  while (true) {
    int client_fd = server_.accept_client();
    if (client_fd < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }
      return;
    }

    if (!setFdnonBlocking(client_fd)) {
      close(client_fd);
      continue;
    }

    poll_fds_.push_back({client_fd, POLLIN, 0});

    std::cout << "new client fd = " << client_fd << std::endl;
  }
}

bool Poller::handle_client_event(size_t idx) {

  int fd = poll_fds_[idx].fd;
  short ev = poll_fds_[idx].revents;

  if (ev & (POLLERR | POLLNVAL)) {
    close_clientfd(idx);
    return true;
  }

  if (ev & POLLIN) {

    char buffer[4096];

    ssize_t n = read(fd, buffer, sizeof(buffer));

    if (n > 0) {
      std::string req(buffer, static_cast<size_t>(n));
      std::string resp = handle_business(fd, req);
      if (send_response(idx, resp)) {
        return true;
      }
    } else if (n == 0) {
      close_clientfd(idx);
      return true;
    } else {
      if (errno == EINTR) {
        return false;
      }

      if (errno == EAGAIN || errno == EWOULDBLOCK) {

      } else {
        close_clientfd(idx);
        return true;
      }
    }
  }

  if (ev & POLLHUP) {
    close_clientfd(idx);
    return true;
  }

  if (ev & POLLOUT) {
    if (send_response(idx, "")) {
      return true;
    }
  }
  return false;
}

bool Poller::send_response(size_t idx, const std::string &response) {
  int fd = poll_fds_[idx].fd;

  if (!response.empty()) {
    write_buffers_[fd].append(response);
  }

  auto it = write_buffers_.find(fd);
  if (it == write_buffers_.end() || it->second.empty()) {
    poll_fds_[idx].events &= ~POLLOUT;
    return false;
  }

  std::string &pending = it->second;

  ssize_t n = shortWrite(fd, pending.data(), pending.size());

  if (n < 0) {
    close_clientfd(idx);
    return true;
  }

  size_t written = static_cast<size_t>(n);

  if (written == pending.size()) {
    write_buffers_.erase(fd);
    poll_fds_[idx].events &= ~POLLOUT;
    return false;
  }

  if (written > 0) {
    pending.erase(0, written);
  }

  // written == 0 或只写了一部分：
  // 说明仍有数据没写完，继续监听 POLLOUT。
  poll_fds_[idx].events |= POLLOUT;
  return false;
}

void Poller::close_clientfd(size_t idx) {
  int fd = poll_fds_[idx].fd;
  if (poll_fds_[idx].fd >= 0) {
    close(poll_fds_[idx].fd);
    poll_fds_[idx].fd = -1;
    std::cout << "clientfd " << fd << " closed" << std::endl;
  }

  write_buffers_.erase(fd);
  poll_fds_.erase(poll_fds_.begin() + static_cast<long>(idx));
}