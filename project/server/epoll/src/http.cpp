#include <algorithm>
#include <cctype>
#include <cstddef>
#include <http.h>
#include <string>

static constexpr size_t kMaxRequestHeadSize = 8192;

ParseResult try_parse_http_request(const std::string &buffer) {
  ParseResult result;
  result.status = ParseStatus::Incomplete;
  result.consumed = 0;

  // 1. 找到 HTTP 头部区域结束：请求行 + headers + 空行
  size_t head_end_pos = buffer.find("\r\n\r\n");
  if (head_end_pos == std::string::npos) {
    return result; // 请求还没收完整
  }

  if (buffer.size() > kMaxRequestHeadSize &&
      buffer.find("\r\n\r\n") == std::string::npos) {
    result.status = ParseStatus::Error;
    return result;
  }

  size_t consumed = head_end_pos + 4;

  // 2. 请求行
  size_t line_start = 0;
  size_t line_end = buffer.find("\r\n");
  if (line_end == std::string::npos || line_end > head_end_pos) {
    result.status = ParseStatus::Error;
    return result;
  }

  std::string request_line = buffer.substr(0, line_end);

  // 3. 请求行格式：METHOD SP PATH SP VERSION
  size_t first_space = request_line.find(' ');
  if (first_space == std::string::npos) {
    result.status = ParseStatus::Error;
    return result;
  }

  size_t second_space = request_line.find(' ', first_space + 1);
  if (second_space == std::string::npos) {
    result.status = ParseStatus::Error;
    return result;
  }

  HttpRequest req;
  req.method = request_line.substr(0, first_space);
  req.path =
      request_line.substr(first_space + 1, second_space - first_space - 1);
  req.version = request_line.substr(second_space + 1);

  if (req.method.empty() || req.path.empty() || req.version.empty()) {
    result.status = ParseStatus::Error;
    return result;
  }

  if (req.version != "HTTP/1.0" && req.version != "HTTP/1.1") {
    result.status = ParseStatus::Error;
    return result;
  }

  // 请求头
  line_start = line_end + 2;

  while (line_start < head_end_pos) {
    line_end = buffer.find("\r\n", line_start);
    if (line_end == std::string::npos || line_end > head_end_pos) {
      result.status = ParseStatus::Error;
      return result;
    }

    std::string line = buffer.substr(line_start, line_end - line_start);

    if (line.empty()) {
      break;
    }

    size_t colon_pos = line.find(":");
    if (colon_pos == std::string::npos) {
      result.status = ParseStatus::Error;
      return result;
    }

    std::string key = line.substr(0, colon_pos);
    std::string value = line.substr(colon_pos + 1);

    key = to_lower(trim(key));
    value = trim(value);

    if (key.empty()) {
      result.status = ParseStatus::Error;
      return result;
    }

    req.headers[key] = value;

    line_start = line_end + 2;
  }

  result.status = ParseStatus::Complete;
  result.request = req;
  result.consumed = consumed;
  return result;
}

std::string make_http_response(int status_code, const std::string &body,
                               const std::string &content_type,
                               bool keep_alive) {
  std::string response;

  response += "HTTP/1.1";
  response += " ";
  response += std::to_string(status_code);
  response += " ";
  response += status_message(status_code);
  response += "\r\n";

  response += "Content-Length:";
  response += " ";
  response += std::to_string(body.size());
  response += "\r\n";

  response += "Content-Type:";
  response += " ";
  response += content_type;
  response += "\r\n";

  if (keep_alive) {
    response += "Connection: keep-alive\r\n";
  } else {
    response += "Connection: close\r\n";
  }

  response += "\r\n";

  response += body;
  return response;
}

std::string handle_http_request(const HttpRequest &req, bool keep_alive) {
  if (req.method != "GET") {
    return make_http_response(405, "Method Not Allowed\n", "text/plain", false);
  }

  if (req.path == "/") {
    return make_http_response(200, "Hello, world!\n", "text/plain", keep_alive);
  }

  if (req.path == "/ping") {
    return make_http_response(200, "pong\n", "text/plain", keep_alive);
  }

  if (req.path == "/status") {
    return make_http_response(200, "OK\n", "text/plain", keep_alive);
  }

  return make_http_response(404, "Not Found\n", "text/plain", false);
}

std::string get_header(const HttpRequest &req, const std::string &key) {
  std::string lower_key = to_lower(key);

  auto it = req.headers.find(lower_key);
  if (it == req.headers.end()) {
    return "";
  }

  return it->second;
}

bool should_keep_alive(const HttpRequest &req) {
  std::string connection = to_lower(get_header(req, "Connection"));

  if (req.version == "HTTP/1.1") {
    return connection != "close";
  }

  if (req.version == "HTTP/1.0") {
    return connection == "keep-alive";
  }

  return false;
}

std::string to_lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });

  return s;
}

std::string trim(const std::string &s) {
  size_t left = 0;
  while (left < s.size() && std::isspace(static_cast<unsigned char>(s[left]))) {
    ++left;
  }

  size_t right = s.size();
  while (right > left &&
         std::isspace(static_cast<unsigned char>(s[right - 1]))) {
    --right;
  }

  return s.substr(left, right - left);
}

std::string status_message(int status_code) {
  switch (status_code) {
  case 200:
    return "OK";
  case 404:
    return "Not Found";
  case 500:
    return "Internal Server Error";
  case 405:
    return "Method Not Allowed";
  default:
    return "Unknown";
  }
}