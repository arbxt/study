#pragma once

#include <string>
class Tcpserver {
public:
  Tcpserver(const std::string &ip, int port);
  ~Tcpserver();

  Tcpserver(const Tcpserver &) = delete;
  Tcpserver &operator=(const Tcpserver &) = delete;

  bool start();
  int accept_client();
  int listen_fd() const;

private:
  std::string ip_;
  int port_;
  int listenfd_;
};