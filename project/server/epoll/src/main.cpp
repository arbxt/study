#include "Epoll.h"
#include "server.h"

#include <iostream>

int main() {
  Tcpserver server("127.0.0.1", 8080);

  if (!server.start()) {
    std::cerr << "server start failed" << std::endl;
    return 1;
  }

  Epoller epoller(server);
  epoller.loop();
  return 0;
}