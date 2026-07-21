#include "json.h"

namespace {
std::string escape_json_string(const std::string &input) {
  std::string result;

  for (char c : input) {
    switch (c) {
    case '"':
      result += "\\\"";
      break;

    case '\\':
      result += "\\\\";
      break;

    case '\n':
      result += "\\n";
      break;

    case '\r':
      result += "\\r";
      break;

    case '\t':
      result += "\\t";
      break;

    default:
      result += c;
      break;
    }
  }

  return result;
}

std::string unescape_json_string(const std::string &input) {
  std::string result;

  for (size_t i = 0; i < input.size(); i++) {
    char c = input[i];

    if (c != '\\') {
      result += c;
      continue;
    }

    // 遇到转义符
    if (i + 1 >= input.size()) {
      return {};
    }

    char next = input[++i];

    switch (next) {
    case '"':
      result += '"';
      break;

    case '\\':
      result += '\\';
      break;

    case 'n':
      result += '\n';
      break;

    case 'r':
      result += '\r';
      break;

    case 't':
      result += '\t';
      break;

    default:
      // 非法转义
      return {};
    }
  }

  return result;
}

bool parse_json_string(const std::string &s, size_t start, std::string &out) {
  if (s[start] != '"')
    return false;

  std::string raw;

  for (size_t i = start + 1; i < s.size(); i++) {
    char c = s[i];

    if (c == '\\') {
      if (i + 1 >= s.size())
        return false;

      raw += c;
      raw += s[++i];

      continue;
    }

    if (c == '"') {
      out = unescape_json_string(raw);
      return true;
    }

    raw += c;
  }

  return false;
}
} // namespace

bool parse_item(const std::string &json, Item &item) {
  size_t pos = 0;

  // 找name字段
  if (!find_key(json, "name", pos)) {
    return false;
  }

  // 跳过 :
  skip_space(json, pos);

  if (json[pos] != ':') {
    return false;
  }

  pos++;

  skip_space(json, pos);

  std::string name;

  if (!parse_json_string(json, pos, name)) {
    return false;
  }

  item.name = name;

  return true;
}

std::string Json::serialize_item(const Item &item) {
  std::string json;

  json += "{";
  json += "\"id\":";
  json += std::to_string(item.id);
  json += ",";

  json += "\"name\":\"";
  json += escape_json_string(item.name);
  json += "\"";

  json += "}";

  return json;
}

std::string Json::serialize_items(const std::vector<Item> &items) {
  std::string json;

  json += "[";

  for (size_t i = 0; i < items.size(); ++i) {
    json += serialize_item(items[i]);

    if (i + 1 < items.size()) {
      json += ",";
    }
  }

  json += "]";

  return json;
}