#include "core/SelectionMask.h"

#include <algorithm>

#include "core/Tile.h"

namespace tuxels {

namespace {

// For each tile in `[x0..x1) × [y0..y1)`, invoke `body(tile, lx0, ly0, lx1, ly1)`
// with the tile-local subrect. `getOrCreate`-style: creates tiles on demand.
template <typename Body>
void forEachTileSubrect(TuxImage& img, int x0, int y0, int x1, int y1,
                        Body&& body) {
  if (x1 <= x0 || y1 <= y0) return;
  const int cx0 = std::max(0, x0);
  const int cy0 = std::max(0, y0);
  const int cx1 = std::min(img.width(), x1);
  const int cy1 = std::min(img.height(), y1);
  if (cx1 <= cx0 || cy1 <= cy0) return;

  const TileCoord tl = tileCoordForPixel(cx0, cy0);
  const TileCoord br = tileCoordForPixel(cx1 - 1, cy1 - 1);
  for (int ty = tl.ty; ty <= br.ty; ++ty) {
    for (int tx = tl.tx; tx <= br.tx; ++tx) {
      Tile* t = img.tiles().getOrCreate({tx, ty});
      const int tileX0 = tx * kTilePx;
      const int tileY0 = ty * kTilePx;
      const int lx0 = std::max(0, cx0 - tileX0);
      const int ly0 = std::max(0, cy0 - tileY0);
      const int lx1 = std::min(kTilePx, cx1 - tileX0);
      const int ly1 = std::min(kTilePx, cy1 - tileY0);
      body(*t, lx0, ly0, lx1, ly1);
    }
  }
}

}  // namespace

void SelectionMask::fillRect(Rect r, float value) {
  value = std::clamp(value, 0.f, 1.f);
  const Rgba32F px{value, 0.f, 0.f, 1.f};
  forEachTileSubrect(image_, r.x, r.y, r.right(), r.bottom(),
                     [&](Tile& t, int lx0, int ly0, int lx1, int ly1) {
                       for (int y = ly0; y < ly1; ++y) {
                         for (int x = lx0; x < lx1; ++x) {
                           t.at(x, y) = px;
                         }
                       }
                     });
}

void SelectionMask::combine(const SelectionMask& other, SelectionMode mode) {
  // Iterate over the document tile-by-tile; either side may have absent
  // tiles (interpreted as 0). For Subtract / Intersect where b==0 leaves
  // self unchanged we skip tiles absent from other.
  const int w = image_.width();
  const int h = image_.height();
  if (w <= 0 || h <= 0) return;
  const TileCoord tl = tileCoordForPixel(0, 0);
  const TileCoord br = tileCoordForPixel(w - 1, h - 1);

  for (int ty = tl.ty; ty <= br.ty; ++ty) {
    for (int tx = tl.tx; tx <= br.tx; ++tx) {
      const TileCoord tc{tx, ty};
      const Tile* bTile = other.image_.tiles().find(tc);
      const bool bAbsent = (bTile == nullptr);

      if (bAbsent && (mode == SelectionMode::Subtract ||
                      mode == SelectionMode::Add)) {
        continue;  // a unchanged (subtract by 0, add of 0)
      }
      if (bAbsent && mode == SelectionMode::Intersect) {
        // Intersect with 0 → self tile becomes zero. Simplest: drop it.
        image_.tiles().set(tc, nullptr);
        continue;
      }
      if (bAbsent && mode == SelectionMode::Replace) {
        // Replace with 0.
        image_.tiles().set(tc, nullptr);
        continue;
      }

      Tile* aTile = image_.tiles().getOrCreate(tc);
      const int tileX0 = tx * kTilePx;
      const int tileY0 = ty * kTilePx;
      const int lx1 = std::min(kTilePx, w - tileX0);
      const int ly1 = std::min(kTilePx, h - tileY0);
      for (int y = 0; y < ly1; ++y) {
        for (int x = 0; x < lx1; ++x) {
          const float a = aTile->at(x, y).r;
          const float b = bTile->at(x, y).r;
          float out = 0.f;
          switch (mode) {
            case SelectionMode::Replace: out = b; break;
            case SelectionMode::Add: out = std::max(a, b); break;
            case SelectionMode::Subtract: out = a * (1.f - b); break;
            case SelectionMode::Intersect: out = std::min(a, b); break;
          }
          aTile->at(x, y) = Rgba32F{out, 0.f, 0.f, 1.f};
        }
      }
    }
  }
}

void SelectionMask::invert() {
  const int w = image_.width();
  const int h = image_.height();
  if (w <= 0 || h <= 0) return;
  const TileCoord tl = tileCoordForPixel(0, 0);
  const TileCoord br = tileCoordForPixel(w - 1, h - 1);

  for (int ty = tl.ty; ty <= br.ty; ++ty) {
    for (int tx = tl.tx; tx <= br.tx; ++tx) {
      const TileCoord tc{tx, ty};
      Tile* t = image_.tiles().getOrCreate(tc);
      const int tileX0 = tx * kTilePx;
      const int tileY0 = ty * kTilePx;
      const int lx1 = std::min(kTilePx, w - tileX0);
      const int ly1 = std::min(kTilePx, h - tileY0);
      for (int y = 0; y < ly1; ++y) {
        for (int x = 0; x < lx1; ++x) {
          const float v = t->at(x, y).r;
          t->at(x, y) = Rgba32F{1.f - v, 0.f, 0.f, 1.f};
        }
      }
    }
  }
}

bool SelectionMask::isEmpty(float epsilon) const {
  for (const auto& [tc, ptr] : image_.tiles()) {
    if (!ptr) continue;
    const Rgba32F* d = ptr->data();
    for (int i = 0; i < kTilePixels; ++i) {
      if (d[i].r > epsilon) return false;
    }
  }
  return true;
}

Rect SelectionMask::boundsOfSelected(float threshold) const {
  const int w = image_.width();
  const int h = image_.height();
  if (w <= 0 || h <= 0) return {};
  int minX = w, minY = h, maxX = -1, maxY = -1;
  for (const auto& [tc, ptr] : image_.tiles()) {
    if (!ptr) continue;
    const int tileX0 = tc.tx * kTilePx;
    const int tileY0 = tc.ty * kTilePx;
    const int lx1 = std::min(kTilePx, w - tileX0);
    const int ly1 = std::min(kTilePx, h - tileY0);
    for (int y = 0; y < ly1; ++y) {
      for (int x = 0; x < lx1; ++x) {
        if (ptr->at(x, y).r > threshold) {
          const int gx = tileX0 + x;
          const int gy = tileY0 + y;
          if (gx < minX) minX = gx;
          if (gy < minY) minY = gy;
          if (gx > maxX) maxX = gx;
          if (gy > maxY) maxY = gy;
        }
      }
    }
  }
  if (maxX < minX) return {};
  return Rect{minX, minY, maxX - minX + 1, maxY - minY + 1};
}

std::unique_ptr<SelectionMask> SelectionMask::clone() const {
  auto out = std::make_unique<SelectionMask>(image_.width(), image_.height());
  for (const auto& [tc, ptr] : image_.tiles()) {
    if (ptr) out->image_.tiles().set(tc, ptr->clone());
  }
  return out;
}

std::unique_ptr<SelectionMask> SelectionMask::makeAll(int w, int h) {
  auto m = std::make_unique<SelectionMask>(w, h);
  m->fillRect(Rect{0, 0, w, h}, 1.f);
  return m;
}

}  // namespace tuxels
