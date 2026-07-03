#include <iostream>

enum class ParseStatus { Complete, Incomplete, Error };

struct HttpRequest {
  std::string method;
  std::string path;
  std::string version;
};

struct ParseResult {
  ParseStatus status;
  HttpRequest request;
  size_t consumed = 0;
};

std::string make_http_response(int status_code, const std::string &body,
                               const std::string &content_type);

std::string handle_http_request(const HttpRequest &req);