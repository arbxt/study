#include "config.h"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

namespace {

std::string trim(const std::string &s) {
  size_t left = 0;
  while (left < s.size() && (s[left] == ' ' || s[left] == '\t' ||
                             s[left] == '\r' || s[left] == '\n')) {
    ++left;
  }

  size_t right = s.size();
  while (right > left && (s[right - 1] == ' ' || s[right - 1] == '\t' ||
                          s[right - 1] == '\r' || s[right - 1] == '\n')) {
    --right;
  }

  return s.substr(left, right - left);
}

bool parse_int(const std::string &s, int &out) {
  if (s.empty()) {
    return false;
  }

  char *end = nullptr;
  errno = 0;

  long value = std::strtol(s.c_str(), &end, 10);

  if (errno != 0 || end == s.c_str() || *end != '\0') {
    return false;
  }

  if (value < std::numeric_limits<int>::min() ||
      value > std::numeric_limits<int>::max()) {
    return false;
  }

  out = static_cast<int>(value);
  return true;
}

bool parse_size_t(const std::string &s, size_t &out) {
  if (s.empty()) {
    return false;
  }

  size_t value = 0;

  for (char ch : s) {
    if (ch < '0' || ch > '9') {
      return false;
    }

    size_t digit = static_cast<size_t>(ch - '0');

    if (value > (std::numeric_limits<size_t>::max() - digit) / 10) {
      return false;
    }

    value = value * 10 + digit;
  }

  out = value;
  return true;
}

bool set_config_value(ServerConfig &config, const std::string &key,
                      const std::string &value) {
  if (key == "port") {
    int v = 0;
    if (!parse_int(value, v) || v <= 0 || v > 65535) {
      return false;
    }
    config.port = v;
    return true;
  }

  if (key == "idle_timeout_seconds") {
    int v = 0;
    if (!parse_int(value, v) || v <= 0) {
      return false;
    }
    config.idle_timeout_seconds = v;
    return true;
  }

  if (key == "max_events") {
    int v = 0;
    if (!parse_int(value, v) || v <= 0) {
      return false;
    }
    config.max_events = v;
    return true;
  }

  if (key == "max_requests_per_event") {
    int v = 0;
    if (!parse_int(value, v) || v <= 0) {
      return false;
    }
    config.max_requests_per_event = v;
    return true;
  }

  if (key == "max_request_head_size") {
    size_t v = 0;
    if (!parse_size_t(value, v) || v == 0) {
      return false;
    }
    config.max_request_head_size = v;
    return true;
  }

  if (key == "max_request_body_size") {
    size_t v = 0;
    if (!parse_size_t(value, v)) {
      return false;
    }
    config.max_request_body_size = v;
    return true;
  }

  std::cerr << "[config] unknown key: " << key << std::endl;
  return true;
}

} // namespace

bool load_config(const std::string &filename, ServerConfig &config) {
  std::ifstream file(filename.c_str());

  if (!file.is_open()) {
    std::cerr << "[config] cannot open " << filename << ", use default config"
              << std::endl;
    return false;
  }

  std::string line;
  int line_no = 0;

  while (std::getline(file, line)) {
    ++line_no;

    line = trim(line);

    if (line.empty()) {
      continue;
    }

    if (line[0] == '#') {
      continue;
    }

    size_t eq_pos = line.find('=');
    if (eq_pos == std::string::npos) {
      std::cerr << "[config] invalid line " << line_no << ": " << line
                << std::endl;
      return false;
    }

    std::string key = trim(line.substr(0, eq_pos));
    std::string value = trim(line.substr(eq_pos + 1));

    if (key.empty() || value.empty()) {
      std::cerr << "[config] empty key or value at line " << line_no
                << std::endl;
      return false;
    }

    if (!set_config_value(config, key, value)) {
      std::cerr << "[config] invalid value at line " << line_no << ": " << line
                << std::endl;
      return false;
    }
  }

  return true;
}