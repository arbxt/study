#include "item_service.h"
#include "item.h"
#include <string>
#include <vector>

ItemService &ItemService::instance() {
  static ItemService service;
  return service;
}

int ItemService::add_item(const std::string &name) {
  if (name.empty()) {
    return -1;
  }

  Item item;

  item.id = next_id_++;
  item.name = name;

  item_.push_back(item);

  return item.id;
}

std::vector<Item> ItemService::get_items() const { return item_; }