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

ParseResult try_parse_http_request(const std::string &buffer);

std::string make_http_response(int status_code, const std::string &body,
                               const std::string &content_type,
                               bool keep_alive);

std::string status_message(int status_code);

bool should_keep_alive(const HttpRequest &req);

std::string get_header(const HttpRequest &req, const std::string &key);

std::string to_lower(std::string s);

std::string trim(const std::string &s);