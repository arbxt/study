#pragma once

#include <string>
#include <vector>

#include "item.h"

namespace Json {

bool parse_item(const std::string &json, Item &item);

std::string serialize_item(const Item &item);

std::string serialize_items(const std::vector<Item> &items);

} // namespace Json