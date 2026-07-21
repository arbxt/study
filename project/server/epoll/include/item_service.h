#pragma once
#include "item.h"

#include <string>
#include <vector>

class ItemService {
public:
  static ItemService &instance();

  int add_item(const std::string &name);
  std::vector<Item> get_items() const;

private:
  ItemService() = default;

  ItemService(const ItemService &) = delete;
  ItemService &operator=(const ItemService &) = delete;

private:
  std::vector<Item> item_;
  int next_id_ = 1;
};