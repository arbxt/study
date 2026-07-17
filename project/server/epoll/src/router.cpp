#include "router.h"
#include "http.h"
#include "item_service.h"
#include "json.h"

#include <unordered_map>

namespace {

using Handler = std::string (*)(const HttpRequest &req, bool keep_alive);

using MethodTable = std::unordered_map<std::string, Handler>;
using RouteTable = std::unordered_map<std::string, MethodTable>;

std::string handle_root(const HttpRequest &req, bool keep_alive) {
  (void)req;

  return make_http_response(200, "Hello, world!\n", "text/plain", keep_alive);
}

std::string handle_ping(const HttpRequest &req, bool keep_alive) {
  (void)req;

  return make_http_response(200, "pong\n", "text/plain", keep_alive);
}

std::string handle_status(const HttpRequest &req, bool keep_alive) {
  (void)req;

  return make_http_response(200, "OK\n", "text/plain", keep_alive);
}

std::string handle_echo(const HttpRequest &req, bool keep_alive) {
  return make_http_response(200, req.body, "text/plain", keep_alive);
}

std::string handle_api_echo(const HttpRequest &req, bool keep_alive) {
  std::string body = "{\"echo\":\"" + req.body + "\"}\n";

  return make_http_response(200, body, "application/json", keep_alive);
}

std::string not_found(bool keep_alive) {
  return make_http_response(404, "Not Found\n", "text/plain", keep_alive);
}

std::string method_not_allowed(bool keep_alive) {
  return make_http_response(405, "Method Not Allowed\n", "text/plain",
                            keep_alive);
}

std::string handle_add_item(const HttpRequest &req, bool keep_alive) {
  Item item;

  if (!Json::parse_item(req.body, item)) {
    return make_http_response(400, "{\"error\":\"invalid json\"}",
                              "application/json", keep_alive);
  }

  ItemService::instance().add_item(item);

  return make_http_response(201, Json::serialize_item(item), "application/json",
                            keep_alive);
}

std::string handle_get_items(const HttpRequest &req, bool keep_alive) {
  auto items = ItemService::instance().get_items();

  return make_http_response(200, Json::serialize_items(items),
                            "application/json", keep_alive);
}

const RouteTable &routes() {
  static const RouteTable table = [] {
    RouteTable t;

    t["/"]["GET"] = handle_root;
    t["/ping"]["GET"] = handle_ping;
    t["/status"]["GET"] = handle_status;
    t["/echo"]["POST"] = handle_echo;
    t["/api/echo"]["POST"] = handle_api_echo;
    t["/api/items"]["GET"] = handle_get_items;
    t["/api/items"]["POST"] = handle_add_item;

    return t;
  }();

  return table;
}

} // namespace

std::string handle_http_request(const HttpRequest &req, bool keep_alive) {
  const RouteTable &table = routes();

  auto path_it = table.find(req.path);

  if (path_it == table.end()) {
    std::cout << "[router] 404 method=" << req.method << " path=" << req.path
              << std::endl;

    return not_found(keep_alive);
  }

  const MethodTable &methods = path_it->second;

  auto method_it = methods.find(req.method);

  if (method_it == methods.end()) {
    std::cout << "[router] 405 method=" << req.method << " path=" << req.path
              << std::endl;

    return method_not_allowed(keep_alive);
  }

  std::cout << "[router] dispatch method=" << req.method << " path=" << req.path
            << std::endl;

  Handler handler = method_it->second;
  return handler(req, keep_alive);
}