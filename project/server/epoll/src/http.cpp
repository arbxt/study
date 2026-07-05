#include <http.h>
#include <string>

std::string make_http_response(int status_code, const std::string &body,
                               const std::string &content_type) {
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

  response += "Connection:";
  response += " ";
  response += "close";
  response += "\r\n";

  response += "\r\n";

  response += body;
  return response;
}

std::string handle_http_request(const HttpRequest &req) {
  if (req.method != "GET") {
    return make_http_response(405, "Method Not Allowed\n", "text/plain");
  }

  if (req.path == "/") {
    return make_http_response(200, "Hello, world!\n", "text/plain");
  }

  if (req.path == "/ping") {
    return make_http_response(200, "pong\n", "text/plain");
  }

  if (req.path == "/status") {
    return make_http_response(200, "OK\n", "text/plain");
  }

  return make_http_response(404, "Not Found\n", "text/plain");
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

ParseResult try_parse_http_request(const std::string &buffer) {
  ParseResult result;
  result.status = ParseStatus::Incomplete;
  result.consumed = 0;

  // 1. 找到 HTTP 头部区域结束：请求行 + headers + 空行
  size_t head_end_pos = buffer.find("\r\n\r\n");
  if (head_end_pos == std::string::npos) {
    return result; // 请求还没收完整
  }

  size_t consumed = head_end_pos + 4;

  // 2. 找请求行结束
  size_t request_line_end = buffer.find("\r\n");
  if (request_line_end == std::string::npos ||
      request_line_end > head_end_pos) {
    result.status = ParseStatus::Error;
    return result;
  }

  std::string request_line = buffer.substr(0, request_line_end);

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

  result.status = ParseStatus::Complete;
  result.request = req;
  result.consumed = consumed;
  return result;
}