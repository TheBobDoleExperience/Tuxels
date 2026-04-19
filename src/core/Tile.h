#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "core/Pixel.h"

namespace tuxels {

inline constexpr int kTilePx = 256;
inline constexpr int kTilePixels = kTilePx * kTilePx;

struct TileCoord {
  int tx = 0;
  int ty = 0;
  friend constexpr bool operator==(const TileCoord&, const TileCoord&) = default;
};

struct TileCoordHash {
  std::size_t operator()(TileCoord tc) const noexcept {
    const uint64_t packed =
        (static_cast<uint64_t>(static_cast<uint32_t>(tc.tx)) << 32) |
        static_cast<uint64_t>(static_cast<uint32_t>(tc.ty));
    return std::hash<uint64_t>{}(packed);
  }
};

class Tile {
 public:
  Tile() : data_(std::make_unique<Rgba32F[]>(kTilePixels)) {}

  static std::shared_ptr<Tile> makeFilled(Rgba32F c) {
    auto t = std::make_shared<Tile>();
    t->fill(c);
    return t;
  }

  Rgba32F* data() noexcept { return data_.get(); }
  const Rgba32F* data() const noexcept { return data_.get(); }

  Rgba32F& at(int localX, int localY) noexcept {
    return data_[localY * kTilePx + localX];
  }
  const Rgba32F& at(int localX, int localY) const noexcept {
    return data_[localY * kTilePx + localX];
  }

  void fill(Rgba32F c) {
    for (int i = 0; i < kTilePixels; ++i) data_[i] = c;
  }

  std::shared_ptr<Tile> clone() const {
    auto out = std::make_shared<Tile>();
    for (int i = 0; i < kTilePixels; ++i) out->data_[i] = data_[i];
    return out;
  }

 private:
  std::unique_ptr<Rgba32F[]> data_;
};

inline TileCoord tileCoordForPixel(int x, int y) {
  auto divFloor = [](int a, int b) {
    return (a / b) - (((a % b) != 0) && ((a ^ b) < 0) ? 1 : 0);
  };
  return {divFloor(x, kTilePx), divFloor(y, kTilePx)};
}

inline int localInTile(int pixelCoord) {
  int m = pixelCoord % kTilePx;
  if (m < 0) m += kTilePx;
  return m;
}

}  // namespace tuxels
