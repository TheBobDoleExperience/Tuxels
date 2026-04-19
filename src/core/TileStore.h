#pragma once

#include <memory>
#include <unordered_map>
#include <utility>

#include "core/Tile.h"

namespace tuxels {

class TileStore {
 public:
  using Map = std::unordered_map<TileCoord, std::shared_ptr<Tile>, TileCoordHash>;

  TileStore() = default;

  const Tile* find(TileCoord tc) const {
    auto it = map_.find(tc);
    return it == map_.end() ? nullptr : it->second.get();
  }

  std::shared_ptr<Tile> sharedAt(TileCoord tc) const {
    auto it = map_.find(tc);
    return it == map_.end() ? nullptr : it->second;
  }

  Tile* getOrCreate(TileCoord tc) {
    auto [it, inserted] = map_.try_emplace(tc);
    if (inserted) it->second = std::make_shared<Tile>();
    return it->second.get();
  }

  void set(TileCoord tc, std::shared_ptr<Tile> tile) {
    if (tile) {
      map_[tc] = std::move(tile);
    } else {
      map_.erase(tc);
    }
  }

  std::shared_ptr<Tile> takeShared(TileCoord tc) {
    auto it = map_.find(tc);
    if (it == map_.end()) return nullptr;
    return it->second;
  }

  bool contains(TileCoord tc) const { return map_.count(tc) != 0; }

  std::size_t size() const noexcept { return map_.size(); }
  bool empty() const noexcept { return map_.empty(); }

  void clear() { map_.clear(); }

  Map::iterator begin() { return map_.begin(); }
  Map::iterator end()   { return map_.end(); }
  Map::const_iterator begin() const { return map_.begin(); }
  Map::const_iterator end()   const { return map_.end(); }

 private:
  Map map_;
};

}  // namespace tuxels
