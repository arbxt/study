#include "http.h"

#include <cassert>
#include <iostream>
#include <string>

void test_parse_complete_get() {
  std::string raw = "GET /ping HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "\r\n";

  ParseResult r = try_parse_http_request(raw);

  assert(r.status == ParseStatus::Complete);
  assert(r.request.method == "GET");
  assert(r.request.path == "/ping");
  assert(r.request.version == "HTTP/1.1");
  assert(r.consumed == raw.size());
}

void test_parse_incomplete() {
  std::string raw = "GET /ping HTTP/1.1\r\n"
                    "Host: localhost\r\n";

  ParseResult r = try_parse_http_request(raw);

  assert(r.status == ParseStatus::Incomplete);
}

void test_parse_bad_request_line() {
  std::string raw = "BADREQUEST\r\n"
                    "\r\n";

  ParseResult r = try_parse_http_request(raw);

  assert(r.status == ParseStatus::Error);
}

void test_parse_empty_request_line() {
  std::string raw = "\r\n"
                    "Host: localhost\r\n"
                    "\r\n";

  ParseResult r = try_parse_http_request(raw);

  assert(r.status == ParseStatus::Error);
}

void test_make_response() {
  std::string resp = make_http_response(200, "pong\n", "text/plain");
  std::cout << "===resp start===\n" << resp << "\n===resp end===\n";
  assert(resp.find("HTTP/1.1 200 OK\r\n") == 0);
  assert(resp.find("Content-Length: 5\r\n") != std::string::npos);
  assert(resp.find("Content-Type: text/plain\r\n") != std::string::npos);
  assert(resp.find("Connection: close\r\n") != std::string::npos);
  assert(resp.find("\r\n\r\npong\n") != std::string::npos);
}

void test_handle_http_request() {
  HttpRequest req;
  req.method = "GET";
  req.path = "/ping";
  req.version = "HTTP/1.1";

  std::string resp = handle_http_request(req);

  assert(resp.find("HTTP/1.1 200 OK\r\n") == 0);
  assert(resp.find("\r\n\r\npong\n") != std::string::npos);
}

void test_404() {
  HttpRequest req;
  req.method = "GET";
  req.path = "/notfound";
  req.version = "HTTP/1.1";

  std::string resp = handle_http_request(req);

  assert(resp.find("HTTP/1.1 404 Not Found\r\n") == 0);
  assert(resp.find("\r\n\r\nNot Found\n") != std::string::npos);
}

void test_405() {
  HttpRequest req;
  req.method = "POST";
  req.path = "/ping";
  req.version = "HTTP/1.1";

  std::string resp = handle_http_request(req);

  assert(resp.find("HTTP/1.1 405 Method Not Allowed\r\n") == 0);
}

int main() {
  test_parse_complete_get();
  test_parse_incomplete();
  test_parse_bad_request_line();
  test_parse_empty_request_line();
  test_make_response();
  test_handle_http_request();
  test_404();
  test_405();

  std::cout << "all http v1 tests passed\n";
  return 0;
}