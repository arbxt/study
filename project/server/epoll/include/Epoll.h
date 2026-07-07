#pragma once

#include "server.h"

#include <cstdint>
#include <ctime>
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

    time_t last_active = 0;
  };

private:
  Tcpserver &server_;
  int listen_fd_;
  int epoll_fd_;

  std::vector<epoll_event> events_;
  std::unordered_map<int, Connection> connections_;

private:
  bool add_fd(int fd, uint32_t events);
  void mod_fd(int fd, uint32_t events);
  void del_fd(int fd);

  void handle_new_connection();
  void handle_client_events(int fd, uint32_t events);
  void check_timeout_connections();
  bool process_http_buffer(int fd, Connection &conn);
  bool send_response(int fd, const std::string &response);
  void close_client(int fd);

  void enable_write(int fd);
  void disable_write(int fd);
};
