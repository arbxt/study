#include "http.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <limits>
#include <string>

namespace {

bool parse_request_line(const std::string &line, HttpRequest &req) {
  size_t first_space = line.find(' ');
  if (first_space == std::string::npos) {

    return false;
  }

  size_t second_space = line.find(' ', first_space + 1);
  if (second_space == std::string::npos) {
    return false;
  }
  req.method = line.substr(0, first_space);
  req.path = line.substr(first_space + 1, second_space - first_space - 1);
  req.version = line.substr(second_space + 1);

  if (req.method.empty() || req.path.empty() || req.version.empty()) {
    return false;
  }

  if (req.version != "HTTP/1.0" && req.version != "HTTP/1.1") {
    return false;
  }

  return true;
}

bool parse_header(const std::string &head_part, HttpRequest &req) {
  size_t line_start(0);
  size_t line_end(0);
  size_t end_pos(head_part.size());

  while (line_start < end_pos) {
    line_end = head_part.find("\r\n", line_start);
    if (line_end == std::string::npos || line_end > end_pos) {
      return false;
    }
    std::string line = head_part.substr(line_start, line_end - line_start);

    if (line.empty()) {
      break;
    }

    size_t colon_pos = line.find(":");
    if (colon_pos == std::string::npos) {
      return false;
    }

    std::string key = line.substr(0, colon_pos);
    std::string value = line.substr(colon_pos + 1);

    key = to_lower(trim(key));
    value = trim(value);

    if (key.empty()) {
      return false;
    }

    req.headers[key] = value;

    line_start = line_end + 2;
  }
  return true;
}

/*
 bool parse_content_length(const std::string &s, size_t &out) {
  if (s.empty())
    return false;

  // 不允许负数
  if (s[0] == '-')
    return false;

  try {
    std::size_t pos = 0;

    unsigned long value = std::stoul(s, &pos);

    if (pos != s.size())
      return false;

    out = static_cast<size_t>(value);

    return true;
  } catch (const std::invalid_argument &) {
    return false;
  } catch (const std::out_of_range &) {
    return false;
  }
}
  */

bool parse_content_length(const std::string &s, size_t &out) {
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

} // namespace

ParseResult try_parse_http_request(const std::string &buffer,
                                   size_t max_head_size, size_t max_body_size) {
  ParseResult result;
  result.status = ParseStatus::Incomplete;
  result.consumed = 0;

  // 1. 找到 HTTP 头部区域结束：请求行 + headers + 空行
  size_t head_end_pos = buffer.find("\r\n\r\n");

  if (head_end_pos == std::string::npos) {
    if (buffer.size() > max_head_size) {
      result.status = ParseStatus::Error;
    }
    return result;
  }

  if (head_end_pos > max_head_size) {
    result.status = ParseStatus::Error;
    return result;
  }

  // 2. 请求行
  size_t line_start = 0;
  size_t line_end = buffer.find("\r\n");
  if (line_end == std::string::npos || line_end > head_end_pos) {
    result.status = ParseStatus::Error;
    return result;
  }

  std::string request_line = buffer.substr(line_start, line_end);
  HttpRequest req;

  if (!parse_request_line(request_line, req)) {
    result.status = ParseStatus::Error;
    return result;
  }

  std::string header = buffer.substr(line_end + 2, head_end_pos - line_end);
  // 请求头
  if (!parse_header(header, req)) {
    result.status = ParseStatus::Error;
    return result;
  }

  size_t body_size = 0;

  std::string cl = get_header(req, "content-length");
  if (!cl.empty()) {
    if (!parse_content_length(cl, body_size)) {
      result.status = ParseStatus::Error;
      return result;
    }
  }

  if (req.method == "POST" && cl.empty()) {
    result.status = ParseStatus::Error;
    return result;
  }

  if (body_size > max_body_size) {
    result.status = ParseStatus::Error;
    return result;
  }

  std::string body = buffer.substr(head_end_pos + 4, body_size);

  if (body.size() < body_size) {
    return result;
  }
  req.body = body;

  if (body_size > std::numeric_limits<size_t>::max() - head_end_pos - 4) {
    result.status = ParseStatus::Error;
    return result;
  }

  size_t consumed = head_end_pos + 4 + body_size;

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
  case 400:
    return "Bad Request";
  case 201:
    return "Created";
  case 415:
    return "Unsupported Media Type";
  default:
    return "Unknown";
  }
}
