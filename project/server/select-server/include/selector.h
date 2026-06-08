#pragma once

#include "server.h"
#include <sys/select.h>
#include <vector>

class Selector {
public:
  explicit Selector(TcpServer &tcp_server);

  ~Selector();

  void loop();

private:
  void handle_new_connection();
  void handle_client_event(int client_fd);
  void remove_client(int client_fd);
  void rebuild_max_fd();

private:
  fd_set master_set_;
  fd_set read_set_;
  std::vector<int> client_fds_;
  std::vector<int> closed_fds_;
  TcpServer &tcp_server_;
  int max_fd_;
};