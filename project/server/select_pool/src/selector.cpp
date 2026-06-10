#include "selector.h"
#include "handle.h"
#include "server.h"
#include "threadpool.h"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

Selector::Selector(TcpServer &tcp_server, ThreadPool &pool)
    : tcp_server_(tcp_server), pool_(pool), max_fd_(tcp_server.listen_fd()) {
  FD_ZERO(&master_set_);
  FD_ZERO(&read_set_);
  FD_SET(tcp_server_.listen_fd(), &master_set_);

  std::cout << "Selector 初始化完成，listen_fd = " << tcp_server_.listen_fd()
            << ", max_fd = " << max_fd_ << std::endl;
}

Selector::~Selector() {}

void Selector::loop() {

  while (true) {
    read_set_ = master_set_;

    int ready_count =
        select(max_fd_ + 1, &read_set_, nullptr, nullptr, nullptr);

    if (ready_count == -1) {
      if (errno == EINTR) {
        continue;
      }

      std::cerr << "select 失败: errno = " << errno << ", "
                << std::strerror(errno) << std::endl;
      continue;
    }

    if (FD_ISSET(tcp_server_.listen_fd(), &read_set_)) {

      std::cout << "listen_fd 可读，有新连接" << std::endl;

      handle_new_connection();

      --ready_count;

      if (ready_count <= 0) {
        continue;
      }
    }

    for (int client_fd : client_fds_) {
      if (!FD_ISSET(client_fd, &read_set_)) {
        continue;
      }

      handle_client_event(client_fd);

      --ready_count;

      if (ready_count <= 0) {
        break;
      }
    }

    for (int client_fd : closed_fds_) {
      remove_client(client_fd);
    }
    closed_fds_.clear();
  }
}

void Selector::handle_new_connection() {

  int client_fd = tcp_server_.accept_client();

  if (client_fd == -1) {
    return;
  }

  if (client_fd >= FD_SETSIZE) {
    std::cerr << "client_fd 超过 FD_SETSIZE 限制，关闭连接: " << client_fd
              << std::endl;
    close(client_fd);
    return;
  }

  FD_SET(client_fd, &master_set_);
  client_fds_.push_back(client_fd);

  if (client_fd > max_fd_) {
    max_fd_ = client_fd;
  }

  std::cout << "新客户端加入 select 监听集合，client_fd = " << client_fd
            << std::endl;
}

void Selector::handle_client_event(int client_fd) {
  char buffer[1024];
  std::memset(buffer, 0, sizeof(buffer));
  ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
  if (bytes_read > 0) {
    const std::string data = std::string(buffer, bytes_read);
    pool_.submit([client_fd, data] { handle_business(client_fd, data); });
    return;
  }
  if (bytes_read == 0) {
    closed_fds_.push_back(client_fd);
    return;
  }
  if (errno == EINTR) {
    return;
  }
  closed_fds_.push_back(client_fd);
}

void Selector::remove_client(int client_fd) {
  close(client_fd);
  FD_CLR(client_fd, &master_set_);
  client_fds_.erase(
      std::remove(client_fds_.begin(), client_fds_.end(), client_fd),
      client_fds_.end());
  std::cout << "client_fd" << client_fd << "关闭" << std::endl;
  if (client_fd == max_fd_) {
    rebuild_max_fd();
  }
}

void Selector::rebuild_max_fd() {
  max_fd_ = tcp_server_.listen_fd();
  for (const int client_fd : client_fds_) {
    if (client_fd > max_fd_) {
      max_fd_ = client_fd;
      std::cout << "max_fd_ = " << max_fd_ << std::endl;
    }
  }
}