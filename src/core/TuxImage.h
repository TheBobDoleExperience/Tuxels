#pragma once

#include <algorithm>
#include <functional>
#include <unordered_map>

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
  using TileSnapshot =
      std::unordered_map<TileCoord, std::shared_ptr<Tile>, TileCoordHash>;

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
    Tile* t = tileForWrite(tileCoordForPixel(x, y));
    t->at(localInTile(x), localInTile(y)) = c;
  }

  // Start copy-on-write recording. On first write to each tile during the
  // recording window, the tile's current shared_ptr (or nullptr if absent)
  // is captured and a clone replaces it — so the captured pointer stays
  // frozen as the pre-change snapshot regardless of further mutation.
  void beginRecord() {
    recording_ = true;
    before_.clear();
  }

  // End recording and hand back the pre-change and post-change tile
  // pointers for every tile touched during the window. The returned maps
  // reference the same TileCoords, making swap-based undo/redo trivial.
  struct Recorded {
    TileSnapshot before;
    TileSnapshot after;
  };
  Recorded stopRecord() {
    Recorded r;
    r.before = std::move(before_);
    before_.clear();
    recording_ = false;
    for (const auto& kv : r.before) {
      r.after.emplace(kv.first, tiles_.sharedAt(kv.first));
    }
    return r;
  }

  bool recording() const noexcept { return recording_; }

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
  Tile* tileForWrite(TileCoord tc) {
    if (recording_) {
      auto it = before_.find(tc);
      if (it == before_.end()) {
        auto prev = tiles_.sharedAt(tc);
        before_.emplace(tc, prev);
        std::shared_ptr<Tile> cow =
            prev ? prev->clone() : std::make_shared<Tile>();
        Tile* raw = cow.get();
        tiles_.set(tc, std::move(cow));
        return raw;
      }
    }
    return tiles_.getOrCreate(tc);
  }

  int width_ = 0;
  int height_ = 0;
  TileStore tiles_;
  bool recording_ = false;
  TileSnapshot before_;
};

}  // namespace tuxels
