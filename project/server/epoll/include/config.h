// config.h
#pragma once

#include <cstddef>
#include <string>

struct ServerConfig {
  int port = 8080;
  int idle_timeout_seconds = 60;
  int max_events = 1024;
  int max_requests_per_event = 16;

  size_t max_request_head_size = 8 * 1024;
  size_t max_request_body_size = 1024 * 1024;
};

bool load_config(const std::string &filename, ServerConfig &config);