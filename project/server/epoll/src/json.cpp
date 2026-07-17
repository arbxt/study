#include "json.h"

bool parse_item(const std::string &json, Item &item) {
  auto pos = json.find("\"name\"");

  if (pos == std::string::npos)
    return false;

  auto colon = json.find(':', pos);

  if (colon == std::string::npos)
    return false;

  auto first = json.find('"', colon);

  if (first == std::string::npos)
    return false;

  auto second = json.find('"', first + 1);

  if (second == std::string::npos)
    return false;

  item.name = json.substr(first + 1, second - first - 1);

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

std::string Json::serialize_items(const std::vector<Item> &items) {
  std::string json;

  json += "[";

  for (const auto item : items) {
    json += serialize_item(item);
  }

  json += "]";

  return json;
}