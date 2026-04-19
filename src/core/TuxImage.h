#pragma once

#include <algorithm>
#include <functional>

#include "core/Pixel.h"
#include "core/Tile.h"
#include "core/TileStore.h"

namespace tuxels {

struct Rect {
  int x = 0, y = 0, w = 0, h = 0;
  bool isEmpty() const { return w <= 0 || h <= 0; }
  int right() const { return x + w; }
  int bottom() const { return y + h; }
};

class TuxImage {
 public:
  TuxImage() = default;
  TuxImage(int w, int h) : width_(w), height_(h) {}

  int width() const noexcept { return width_; }
  int height() const noexcept { return height_; }
  Rect bounds() const { return {0, 0, width_, height_}; }

  bool inBounds(int x, int y) const {
    return x >= 0 && y >= 0 && x < width_ && y < height_;
  }

  Rgba32F getPixel(int x, int y) const {
    if (!inBounds(x, y)) return Rgba32F::transparent();
    const Tile* t = tiles_.find(tileCoordForPixel(x, y));
    if (!t) return Rgba32F::transparent();
    return t->at(localInTile(x), localInTile(y));
  }

  void setPixel(int x, int y, Rgba32F c) {
    if (!inBounds(x, y)) return;
    Tile* t = tiles_.getOrCreate(tileCoordForPixel(x, y));
    t->at(localInTile(x), localInTile(y)) = c;
  }

  void fill(Rgba32F c) {
    if (width_ <= 0 || height_ <= 0) return;
    TileCoord tl = tileCoordForPixel(0, 0);
    TileCoord br = tileCoordForPixel(width_ - 1, height_ - 1);
    for (int ty = tl.ty; ty <= br.ty; ++ty) {
      for (int tx = tl.tx; tx <= br.tx; ++tx) {
        Tile* t = tiles_.getOrCreate({tx, ty});
        t->fill(c);
      }
    }
  }

  const TileStore& tiles() const { return tiles_; }
  TileStore& tiles() { return tiles_; }

  Rect tileBounds() const {
    if (width_ <= 0 || height_ <= 0) return {};
    TileCoord tl = tileCoordForPixel(0, 0);
    TileCoord br = tileCoordForPixel(width_ - 1, height_ - 1);
    return {tl.tx, tl.ty, br.tx - tl.tx + 1, br.ty - tl.ty + 1};
  }

  Rect pixelRectForTile(TileCoord tc) const {
    int x0 = tc.tx * kTilePx;
    int y0 = tc.ty * kTilePx;
    int x1 = std::min(x0 + kTilePx, width_);
    int y1 = std::min(y0 + kTilePx, height_);
    return {x0, y0, std::max(0, x1 - x0), std::max(0, y1 - y0)};
  }

 private:
  int width_ = 0;
  int height_ = 0;
  TileStore tiles_;
};

}  // namespace tuxels
