#pragma once

#include "server.h"

#include <string>
#include <sys/epoll.h>
#include <unordered_map>
#include <vector>

class Epoller {
public:
  explicit Epoller(Tcpserver &server);
  ~Epoller();

  Epoller(const Epoller &) = delete;
  Epoller &operator=(const Epoller &) = delete;

  void loop();

private:
  struct Connection {
    std::string read_buffer;
    std::string write_buffer;
    bool close_after_write = false;
  };

private:
  Tcpserver &server_;
  int listen_fd_;
  int epoll_fd_;

  std::vector<epoll_event> events_;
};
