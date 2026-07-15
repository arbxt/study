#include "item_service.h"
#include "item.h"
#include <string>
#include <vector>

ItemService &ItemService::instance() {
  static ItemService service;
  return service;
}

bool ItemService::add_item(const Item &item) {
  Item new_item = item;

  if (new_item.name.empty()) {
    return false;
  }

  new_item.id = next_id_++;

  item_.push_back(new_item);

  return true;
}

std::vector<Item> ItemService::get_items() const { return item_; }