#include "Epoll.h"
#include "config.h"
#include "server.h"

#include <iostream>

int main() {
  ServerConfig config;

  load_config("serverconfig.txt", config);

  std::cout << "[config] port=" << config.port
            << " idle_timeout_seconds=" << config.idle_timeout_seconds
            << " max_events=" << config.max_events
            << " max_requests_per_event=" << config.max_requests_per_event
            << " max_request_head_size=" << config.max_request_head_size
            << " max_request_body_size=" << config.max_request_body_size
            << std::endl;

  Tcpserver server("127.0.0.1", config.port);

  if (!server.start()) {
    std::cerr << "server start failed" << std::endl;
    return 1;
  }

  Epoller epoller(server, config);
  epoller.loop();
  return 0;
}