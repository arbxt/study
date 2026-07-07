#pragma once

#include "http.h"

std::string handle_http_request(const HttpRequest &req, bool keep_alive);