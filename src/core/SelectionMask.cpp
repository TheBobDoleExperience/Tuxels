#include "core/SelectionMask.h"

#include <algorithm>
#include <cmath>

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

void SelectionMask::fillPolygon(const std::vector<Point2f>& points,
                                float value) {
  if (points.size() < 3) return;
  value = std::clamp(value, 0.f, 1.f);
  const Rgba32F px{value, 0.f, 0.f, 1.f};

  // Build the edge table. For each non-horizontal polygon edge we store
  // (yMin, yMax, x at yMin, dx/dy) and always orient yMin < yMax so the
  // scanline loop doesn't care about vertex winding order. Horizontal
  // edges contribute nothing to even-odd parity so we skip them outright.
  struct Edge {
    float yMin;
    float yMax;
    float xAtYMin;
    float invSlope;
  };
  std::vector<Edge> edges;
  edges.reserve(points.size());
  float yMinAll = points[0].y;
  float yMaxAll = points[0].y;
  for (std::size_t i = 0; i < points.size(); ++i) {
    const Point2f& a = points[i];
    const Point2f& b = points[(i + 1) % points.size()];
    yMinAll = std::min(yMinAll, a.y);
    yMaxAll = std::max(yMaxAll, a.y);
    if (a.y == b.y) continue;  // horizontal, skipped
    float x0 = a.x, y0 = a.y, x1 = b.x, y1 = b.y;
    if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); }
    edges.push_back({y0, y1, x0, (x1 - x0) / (y1 - y0)});
  }
  if (edges.empty()) return;

  const int docW = image_.width();
  const int docH = image_.height();
  const int yStart = std::max(0, static_cast<int>(std::floor(yMinAll)));
  const int yEnd =
      std::min(docH, static_cast<int>(std::ceil(yMaxAll)));
  if (yEnd <= yStart) return;

  std::vector<float> xs;
  for (int y = yStart; y < yEnd; ++y) {
    // Sample at pixel center. Edges are half-open in y: active for
    // `scanY ∈ [yMin, yMax)`, so vertices where two edges meet are each
    // counted exactly once — the edge whose yMin == scanY is in; the one
    // ending at yMax == scanY is out. This avoids double-count spikes
    // at shared vertices.
    const float scanY = y + 0.5f;
    xs.clear();
    for (const Edge& e : edges) {
      if (scanY >= e.yMin && scanY < e.yMax) {
        xs.push_back(e.xAtYMin + (scanY - e.yMin) * e.invSlope);
      }
    }
    if (xs.size() < 2) continue;
    std::sort(xs.begin(), xs.end());

    // Walk crossings in pairs (even-odd). For span (xa, xb), a pixel `i`
    // is inside iff its center `i + 0.5` satisfies xa < i + 0.5 < xb
    // (strict on both ends). That gives xStart = floor(xa + 0.5) and
    // xEnd = ceil(xb - 0.5). The asymmetric-looking formulas both reduce
    // to the expected integer span for integer-coord polygons, and
    // correctly exclude a pixel whose center lands exactly on an edge
    // (e.g. the 45° hypotenuse of a right triangle at half-integer x).
    // Clamping to the doc happens at tile-fill time.
    for (std::size_t k = 0; k + 1 < xs.size(); k += 2) {
      const int xa =
          std::max(0, static_cast<int>(std::floor(xs[k] + 0.5f)));
      const int xb =
          std::min(docW, static_cast<int>(std::ceil(xs[k + 1] - 0.5f)));
      if (xb <= xa) continue;
      forEachTileSubrect(image_, xa, y, xb, y + 1,
                         [&](Tile& t, int lx0, int ly0, int lx1, int ly1) {
                           for (int yy = ly0; yy < ly1; ++yy) {
                             for (int xx = lx0; xx < lx1; ++xx) {
                               t.at(xx, yy) = px;
                             }
                           }
                         });
    }
  }
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
