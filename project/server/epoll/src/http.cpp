#include <http.h>

std::string make_http_response(int status_code, const std::string &body,
                               const std::string &content_type) {}

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