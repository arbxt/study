#include "json.h"

bool Json::parse_item(const std::string &json, Item &item) {
  while (true) {
    auto pos =
  }
  return true;
}

std::string Json::serialize_item(const Item &item) {
  std::string json;

  json += "{";
  json += "\"id\":";
  json += std::to_string(item.id);
  json += ",";

  json += "\"name\":\"";
  json += item.name;
  json += "\"";

  json += "}";

  return json;
}

std::string Json::serialize_items(const std::vector<Item> &items) {}