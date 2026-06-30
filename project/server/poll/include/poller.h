#pragma once

#include "server.h"
#include <cstddef>
#include <poll.h>
#include <string>
#include <sys/poll.h>
#include <unordered_map>
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
  bool send_response(size_t idx, const std::string &response);

private:
  struct Connection {
    std::string read_buffer;
    std::string write_buffer;
    bool close_after_write = false;
  };
  TcpServer &server_;
  int listen_fd_;
  std::vector<pollfd> poll_fds_;
  std::unordered_map<int, Connection> connections_;
};