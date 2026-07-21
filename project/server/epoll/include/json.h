#pragma once

#include <string>
#include <vector>

#include "item.h"

namespace Json {

bool parse_json(const std::string &json, std::string &name);

std::string serialize_item(const Item &item);

std::string serialize_items(const std::vector<Item> &items);

} // namespace Json