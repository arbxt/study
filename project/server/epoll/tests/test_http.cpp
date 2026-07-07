#include "http.h"
#include "router.h" // 如果 handle_http_request 还在 http.h 里，就删掉这一行

#include <cstdlib>
#include <iostream>
#include <string>

#define EXPECT_TRUE(expr)                                                      \
  do {                                                                         \
    if (!(expr)) {                                                             \
      std::cerr << "[FAILED] " << __func__ << ":" << __LINE__                  \
                << " EXPECT_TRUE(" #expr ")" << std::endl;                     \
      std::exit(1);                                                            \
    }                                                                          \
  } while (0)

#define EXPECT_EQ(a, b)                                                        \
  do {                                                                         \
    if (!((a) == (b))) {                                                       \
      std::cerr << "[FAILED] " << __func__ << ":" << __LINE__                  \
                << " EXPECT_EQ(" #a ", " #b ")" << std::endl;                  \
      std::cerr << "  left : [" << (a) << "]" << std::endl;                    \
      std::cerr << "  right: [" << (b) << "]" << std::endl;                    \
      std::exit(1);                                                            \
    }                                                                          \
  } while (0)

#define RUN_TEST(fn)                                                           \
  do {                                                                         \
    std::cout << "[RUN ] " #fn << std::endl;                                   \
    fn();                                                                      \
    std::cout << "[PASS] " #fn << std::endl;                                   \
  } while (0)

static std::string status_to_string(ParseStatus status) {
  switch (status) {
  case ParseStatus::Complete:
    return "Complete";
  case ParseStatus::Incomplete:
    return "Incomplete";
  case ParseStatus::Error:
    return "Error";
  }

  return "Unknown";
}

#define EXPECT_STATUS(a, b)                                                    \
  do {                                                                         \
    if (!((a) == (b))) {                                                       \
      std::cerr << "[FAILED] " << __func__ << ":" << __LINE__                  \
                << " EXPECT_STATUS(" #a ", " #b ")" << std::endl;              \
      std::cerr << "  left : [" << status_to_string(a) << "]" << std::endl;    \
      std::cerr << "  right: [" << status_to_string(b) << "]" << std::endl;    \
      std::exit(1);                                                            \
    }                                                                          \
  } while (0)

void test_get_no_body() {
  std::string raw = "GET /ping HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "\r\n";

  ParseResult r = try_parse_http_request(raw);

  EXPECT_STATUS(r.status, ParseStatus::Complete);
  EXPECT_EQ(r.request.method, "GET");
  EXPECT_EQ(r.request.path, "/ping");
  EXPECT_EQ(r.request.version, "HTTP/1.1");
  EXPECT_TRUE(r.request.body.empty());
  EXPECT_EQ(r.consumed, raw.size());
}

void test_post_complete_body() {
  std::string raw = "POST /echo HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "Content-Length: 11\r\n"
                    "Content-Type: text/plain\r\n"
                    "\r\n"
                    "hello world";

  ParseResult r = try_parse_http_request(raw);

  EXPECT_STATUS(r.status, ParseStatus::Complete);
  EXPECT_EQ(r.request.method, "POST");
  EXPECT_EQ(r.request.path, "/echo");
  EXPECT_EQ(r.request.version, "HTTP/1.1");
  EXPECT_EQ(r.request.body, "hello world");
  EXPECT_EQ(r.consumed, raw.size());
}

void test_post_incomplete_body() {
  std::string raw = "POST /echo HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "Content-Length: 11\r\n"
                    "Content-Type: text/plain\r\n"
                    "\r\n"
                    "hello";

  ParseResult r = try_parse_http_request(raw);

  EXPECT_STATUS(r.status, ParseStatus::Incomplete);
}

void test_invalid_content_length_alpha() {
  std::string raw = "POST /echo HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "Content-Length: abc\r\n"
                    "\r\n"
                    "hello";

  ParseResult r = try_parse_http_request(raw);

  EXPECT_STATUS(r.status, ParseStatus::Error);
}

void test_invalid_content_length_negative() {
  std::string raw = "POST /echo HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "Content-Length: -1\r\n"
                    "\r\n"
                    "hello";

  ParseResult r = try_parse_http_request(raw);

  EXPECT_STATUS(r.status, ParseStatus::Error);
}

void test_invalid_content_length_suffix() {
  std::string raw = "POST /echo HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "Content-Length: 12abc\r\n"
                    "\r\n"
                    "hello";

  ParseResult r = try_parse_http_request(raw);

  EXPECT_STATUS(r.status, ParseStatus::Error);
}

void test_post_missing_content_length() {
  std::string raw = "POST /echo HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "\r\n"
                    "hello";

  ParseResult r = try_parse_http_request(raw);

  EXPECT_STATUS(r.status, ParseStatus::Error);
}

void test_content_length_smaller_than_body() {
  std::string raw = "POST /echo HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "Content-Length: 5\r\n"
                    "\r\n"
                    "hello world";

  ParseResult r = try_parse_http_request(raw);

  EXPECT_STATUS(r.status, ParseStatus::Complete);
  EXPECT_EQ(r.request.body, "hello");
  EXPECT_TRUE(r.consumed < raw.size());

  std::string remain = raw.substr(r.consumed);
  EXPECT_EQ(remain, " world");
}

void test_content_length_larger_than_body() {
  std::string raw = "POST /echo HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "Content-Length: 20\r\n"
                    "\r\n"
                    "hello";

  ParseResult r = try_parse_http_request(raw);

  EXPECT_STATUS(r.status, ParseStatus::Incomplete);
}

void test_two_post_requests_in_one_buffer() {
  std::string raw = "POST /echo HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "Content-Length: 5\r\n"
                    "\r\n"
                    "hello"
                    "POST /echo HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "Content-Length: 5\r\n"
                    "\r\n"
                    "world";

  ParseResult r1 = try_parse_http_request(raw);

  EXPECT_STATUS(r1.status, ParseStatus::Complete);
  EXPECT_EQ(r1.request.method, "POST");
  EXPECT_EQ(r1.request.path, "/echo");
  EXPECT_EQ(r1.request.body, "hello");

  raw.erase(0, r1.consumed);

  ParseResult r2 = try_parse_http_request(raw);

  EXPECT_STATUS(r2.status, ParseStatus::Complete);
  EXPECT_EQ(r2.request.method, "POST");
  EXPECT_EQ(r2.request.path, "/echo");
  EXPECT_EQ(r2.request.body, "world");
  EXPECT_EQ(r2.consumed, raw.size());
}

void test_header_case_insensitive() {
  std::string raw = "GET /ping HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "CoNnEcTiOn: close\r\n"
                    "\r\n";

  ParseResult r = try_parse_http_request(raw);

  EXPECT_STATUS(r.status, ParseStatus::Complete);

  std::string connection = get_header(r.request, "connection");
  EXPECT_EQ(connection, "close");

  EXPECT_TRUE(!should_keep_alive(r.request));
}

void test_http11_default_keep_alive() {
  std::string raw = "GET /ping HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "\r\n";

  ParseResult r = try_parse_http_request(raw);

  EXPECT_STATUS(r.status, ParseStatus::Complete);
  EXPECT_TRUE(should_keep_alive(r.request));
}

void test_http10_default_close() {
  std::string raw = "GET /ping HTTP/1.0\r\n"
                    "Host: localhost\r\n"
                    "\r\n";

  ParseResult r = try_parse_http_request(raw);

  EXPECT_STATUS(r.status, ParseStatus::Complete);
  EXPECT_TRUE(!should_keep_alive(r.request));
}

void test_http10_keep_alive_header() {
  std::string raw = "GET /ping HTTP/1.0\r\n"
                    "Host: localhost\r\n"
                    "Connection: keep-alive\r\n"
                    "\r\n";

  ParseResult r = try_parse_http_request(raw);

  EXPECT_STATUS(r.status, ParseStatus::Complete);
  EXPECT_TRUE(should_keep_alive(r.request));
}

void test_make_response_content_length() {
  std::string resp = make_http_response(200, "pong\n", "text/plain", true);

  EXPECT_TRUE(resp.find("HTTP/1.1 200 OK\r\n") == 0);
  EXPECT_TRUE(resp.find("Content-Length: 5\r\n") != std::string::npos);
  EXPECT_TRUE(resp.find("Content-Type: text/plain\r\n") != std::string::npos);
  EXPECT_TRUE(resp.find("Connection: keep-alive\r\n") != std::string::npos);
  EXPECT_TRUE(resp.find("\r\n\r\npong\n") != std::string::npos);
}

void test_router_get_ping() {
  HttpRequest req;
  req.method = "GET";
  req.path = "/ping";
  req.version = "HTTP/1.1";

  std::string resp = handle_http_request(req, true);

  EXPECT_TRUE(resp.find("HTTP/1.1 200 OK\r\n") == 0);
  EXPECT_TRUE(resp.find("Content-Length: 5\r\n") != std::string::npos);
  EXPECT_TRUE(resp.find("\r\n\r\npong\n") != std::string::npos);
}

void test_router_post_echo() {
  HttpRequest req;
  req.method = "POST";
  req.path = "/echo";
  req.version = "HTTP/1.1";
  req.body = "hello world";

  std::string resp = handle_http_request(req, true);

  EXPECT_TRUE(resp.find("HTTP/1.1 200 OK\r\n") == 0);
  EXPECT_TRUE(resp.find("Content-Length: 11\r\n") != std::string::npos);
  EXPECT_TRUE(resp.find("\r\n\r\nhello world") != std::string::npos);
}

void test_router_404() {
  HttpRequest req;
  req.method = "GET";
  req.path = "/unknown";
  req.version = "HTTP/1.1";

  std::string resp = handle_http_request(req, true);

  EXPECT_TRUE(resp.find("HTTP/1.1 404 Not Found\r\n") == 0);
}

void test_router_405() {
  HttpRequest req;
  req.method = "POST";
  req.path = "/ping";
  req.version = "HTTP/1.1";
  req.body = "abc";

  std::string resp = handle_http_request(req, true);

  EXPECT_TRUE(resp.find("HTTP/1.1 405 Method Not Allowed\r\n") == 0);
}

int main() {
  RUN_TEST(test_get_no_body);
  RUN_TEST(test_post_complete_body);
  RUN_TEST(test_post_incomplete_body);

  RUN_TEST(test_invalid_content_length_alpha);
  RUN_TEST(test_invalid_content_length_negative);
  RUN_TEST(test_invalid_content_length_suffix);
  RUN_TEST(test_post_missing_content_length);

  RUN_TEST(test_content_length_smaller_than_body);
  RUN_TEST(test_content_length_larger_than_body);
  RUN_TEST(test_two_post_requests_in_one_buffer);

  RUN_TEST(test_header_case_insensitive);
  RUN_TEST(test_http11_default_keep_alive);
  RUN_TEST(test_http10_default_close);
  RUN_TEST(test_http10_keep_alive_header);

  RUN_TEST(test_make_response_content_length);

  RUN_TEST(test_router_get_ping);
  RUN_TEST(test_router_post_echo);
  RUN_TEST(test_router_404);
  RUN_TEST(test_router_405);

  std::cout << "\nall HTTP v3 unit tests passed\n";
  return 0;
}