#pragma once

#include <string>
#include <vector>

class ItemService {
public:
  static ItemService &instance();

  bool add_item(const std::string &item);
  std::vector<std::string> get_items() const;

private:
  ItemService() = default;

  ItemService(const ItemService &) = delete;
  ItemService &operator=(const ItemService &) = delete;

private:
  std::vector<std::string> item_;
};