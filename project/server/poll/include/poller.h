#pragma once

#include "server.h"
#include <poll.h>
#include <vector>

class Poller {
public:
  explicit Poller(TcpServer &server);

  void loop();

private:
  void handle_new_connection();
  void handle_client_event();
  void close_clientfd();
  bool write_all(int fd, const char *data, size_t length);

private:
  TcpServer &server_;
  std::vector<pollfd> clientfds_;
};