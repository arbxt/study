#include "poller.h"
#include "handle.h"
#include "server.h"

#include <cerrno>
#include <cstddef>
#include <cstring>
#include <errno.h>
#include <iostream>
#include <string>
#include <sys/poll.h>
#include <unistd.h>

Poller::Poller(TcpServer &server) : server_(server) {
  client_fds_.push_back({server_.listen_fd(), POLLIN, 0});
}

Poller::~Poller() {
  for (size_t i = 1; i < client_fds_.size(); ++i) {
    if (client_fds_[i].fd >= 0) {
      close(client_fds_[i].fd);
      client_fds_[i].fd = -1;
    }
  }
}

void Poller::loop() {

  while (true) {
    int ready_count = poll(client_fds_.data(), client_fds_.size(), -1);

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

    if (client_fds_[0].revents & POLLIN) {
      handle_new_connection();
      --ready_count;
    }

    for (size_t i = 1; i < client_fds_.size() && ready_count > 0; ++i) {
      if (client_fds_[i].revents == 0) {
        continue;
      }

      bool removed = handle_client_event(i);
      --ready_count;

      // erase后vector前进一位，--i防止跳过元素
      if (removed) {
        --i;
      }
    }
  }
}

void Poller::handle_new_connection() {

  int client_fd = server_.accept_client();
  if (client_fd < 0) {
    return;
  }
  client_fds_.push_back({client_fd, POLLIN, 0});

  std::cout << "new client fd = " << client_fd << std::endl;
}

bool Poller::handle_client_event(size_t idx) {

  int fd = client_fds_[idx].fd;
  short ev = client_fds_[idx].revents;

  if (ev & (POLLERR | POLLHUP | POLLNVAL)) {
    close_clientfd(idx);
    return true;
  }

  if (ev & POLLIN) {

    char buffer[4096];
    std::memset(buffer, 0, sizeof(buffer));

    size_t n = read(fd, buffer, sizeof(buffer));

    if (n > 0) {
      std::string req(buffer, n);

      std::string resp = handle_business(fd, req);

      if (!write_all(fd, resp.data(), resp.size())) {
        close_clientfd(idx);
        return true;
      }

      return false;
    }

    if (n == 0) {
      close_clientfd(idx);
      return true;
    }

    if (errno == EINTR) {
      return false;
    }

    close_clientfd(idx);
    return true;
  }
  return false;
}

void Poller::close_clientfd(size_t idx) {
  int fd = client_fds_[idx].fd;

  if (fd >= 0) {
    close(fd);
    std::cout << "clientfd" << fd << "closed" << std::endl;
    fd = -1;
  }

  client_fds_.erase(client_fds_.begin() + idx);
}

bool Poller::write_all(int fd, const char *data, size_t length) {
  size_t total_writen = 0;

  while (total_writen < length) {
    ssize_t written = write(fd, data + total_writen, length - total_writen);

    if (written > 0) {
      total_writen += written;
    } else if (written == -1) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
  }
  return true;
}
