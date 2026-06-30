#include "server.h"
#include "utils.h"

#include <asm-generic/socket.h>
#include <cstring>
#include <iostream>

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

Tcpserver::Tcpserver(const std::string &ip, int port)
    : ip_(ip), port_(port), listenfd_(-1) {}

Tcpserver::~Tcpserver() {
  if (listenfd_ != -1) {
    close(listenfd_);
    listenfd_ = -1;
  }
}

bool Tcpserver::start() {
  listenfd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd_ == -1) {
    std::cerr << "socket 创建失败: " << std::strerror(errno) << std::endl;
    return false;
  }

  if (!setFdnonBlocking(listenfd_)) {
    close(listenfd_);
    listenfd_ = -1;
    return false;
  }

  int opt = 1;
  if (setsockopt(listenfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) ==
      -1) {
    std::cerr << "setsockopt 失败: " << std::strerror(errno) << std::endl;
    return false;
  }

  sockaddr_in server_addr;
  std::memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(port_);

  if (bind(listenfd_, reinterpret_cast<sockaddr *>(&server_addr),
           sizeof(server_addr)) == -1) {
    std::cerr << "bind 失败: " << std::strerror(errno) << std::endl;
    return false;
  }

  if (listen(listenfd_, 128) == -1) {
    std::cerr << "listen 失败: " << std::strerror(errno) << std::endl;
    return false;
  }

  std::cout << "服务器启动成功，监听端口 " << port_ << std::endl;

  return true;
}

int Tcpserver::accept_client() {
  sockaddr_in client_addr;
  std::memset(&client_addr, 0, sizeof(client_addr));

  socklen_t client_addr_len = sizeof(client_addr);
  int client_fd = accept(listenfd_, reinterpret_cast<sockaddr *>(&client_addr),
                         &client_addr_len);

  if (client_fd < 0) {
    return -1;
  }

  return client_fd;
}

int Tcpserver::listen_fd() const { return listenfd_; }