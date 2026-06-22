#pragma once

#include "server.h"
#include <poll.h>
#include <vector>

class Poller {
public:
  explicit Poller(TcpServer &server);
  ~Poller();

  void loop();

private:
  void handle_new_connection();
  bool handle_client_event(size_t idx);
  void close_clientfd(size_t idx);
  bool write_all(int fd, const char *data, size_t length);

private:
  TcpServer &server_;
  std::vector<pollfd> client_fds_;
};