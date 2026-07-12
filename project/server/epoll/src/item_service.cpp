#include "item_service.h"
#include <string>
#include <vector>

ItemService &ItemService::instance() {
  static ItemService service;
  return service;
}

bool ItemService::add_item(const std::string &item) {
  if (item.empty()) {
    return false;
  }
  item_.push_back(item);
  return true;
}

std::vector<std::string> ItemService::get_items() const { return item_; }