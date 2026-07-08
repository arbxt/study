#include "router.h"
#include "http.h"

static std::string handle_ping(const HttpRequest &req, bool keep_alive) {
  (void)req;
  return make_http_response(200, "pong\n", "text/plain", keep_alive);
}

static std::string handle_status(const HttpRequest &req, bool keep_alive) {
  (void)req;
  return make_http_response(200, "OK\n", "text/plain", keep_alive);
}

static std::string handle_echo(const HttpRequest &req, bool keep_alive) {
  return make_http_response(200, req.body, "text/plain", keep_alive);
}

static std::string handle_api_echo(const HttpRequest &req, bool keep_alive) {
  std::string body = "{\"echo\":\"" + req.body + "\"}\n";

  return make_http_response(200, body, "application/json", keep_alive);
}

static std::string method_not_allowed(bool keep_alive) {
  return make_http_response(405, "Method Not Allowed", "text/plain",
                            keep_alive);
}

static std::string not_found(bool keep_alive) {
  return make_http_response(404, "Not Found", "text/plain", keep_alive);
}

std::string handle_http_request(const HttpRequest &req, bool keep_alive) {
  if (req.path == "/") {
    if (req.method == "GET") {
      return make_http_response(200, "Hello, world!\n", "text/plain",
                                keep_alive);
    }

    return method_not_allowed(keep_alive);
  }

  if (req.path == "/ping") {
    if (req.method == "GET") {
      return handle_ping(req, keep_alive);
    }

    return method_not_allowed(keep_alive);
  }

  if (req.path == "/status") {
    if (req.method == "GET") {
      return handle_status(req, keep_alive);
    }

    return method_not_allowed(keep_alive);
  }

  if (req.path == "/echo") {
    if (req.method == "POST") {
      return handle_echo(req, keep_alive);
    }

    return method_not_allowed(keep_alive);
  }

  if (req.path == "/api/echo") {
    if (req.method == "POST") {
      return handle_api_echo(req, keep_alive);
    }
  }

  return not_found(keep_alive);
}
