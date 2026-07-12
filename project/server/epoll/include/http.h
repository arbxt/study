#pragma once

#include <iostream>
#include <unordered_map>

enum class ParseStatus { Complete, Incomplete, Error };

struct HttpRequest {
  std::string method;
  std::string path;
  std::string version;
  std::unordered_map<std::string, std::string> headers;
  std::string body;
};

struct ParseResult {
  ParseStatus status;
  HttpRequest request;
  size_t consumed = 0;
};

struct HttpResponse {
  int status_code = 200;
  std::string body;
  std::string content_type = "text/plain";
  bool keep_alive = true;
};

ParseResult try_parse_http_request(const std::string &buffer,
                                   size_t max_head_size, size_t max_body_size);

std::string make_http_response(int status_code, const std::string &body,
                               const std::string &content_type,
                               bool keep_alive);

std::string make_http_response(const HttpResponse &resp);

std::string status_message(int status_code);

bool should_keep_alive(const HttpRequest &req);

std::string get_header(const HttpRequest &req, const std::string &key);

std::string to_lower(std::string s);

std::string trim(const std::string &s);