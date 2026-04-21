#include "core/Histogram.h"

#include <algorithm>
#include <cmath>

#include "core/SelectionMask.h"
#include "core/Tile.h"
#include "core/TuxImage.h"

namespace tuxels {

namespace {

inline int toByte(float v) {
  v = std::clamp(v, 0.f, 1.f);
  return static_cast<int>(std::lround(v * 255.f));
}

}  // namespace

Histogram4x256 computeHistogram(const TuxImage& src,
                                const SelectionMask* clip) {
  Histogram4x256 hist{};
  const int w = src.width();
  const int h = src.height();
  if (w <= 0 || h <= 0) return hist;

  for (const auto& [tc, tilePtr] : src.tiles()) {
    if (!tilePtr) continue;
    const int docX0 = tc.tx * kTilePx;
    const int docY0 = tc.ty * kTilePx;
    // Clip the tile to the document. TuxImage::fill materializes whole
    // tiles even when the doc size isn't a multiple of kTilePx, so the
    // trailing pixels of the edge tile must be skipped.
    const int px0 = std::max(0, -docX0);
    const int py0 = std::max(0, -docY0);
    const int px1 = std::min(kTilePx, w - docX0);
    const int py1 = std::min(kTilePx, h - docY0);
    if (px0 >= px1 || py0 >= py1) continue;

    const Rgba32F* data = tilePtr->data();
    for (int py = py0; py < py1; ++py) {
      for (int px = px0; px < px1; ++px) {
        const Rgba32F& p = data[py * kTilePx + px];
        if (p.a <= 0.f) continue;
        if (clip) {
          const int docX = docX0 + px;
          const int docY = docY0 + py;
          if (clip->sample(docX, docY) < 0.5f) continue;
        }
        ++hist.buckets[0][toByte(p.r)];
        ++hist.buckets[1][toByte(p.g)];
        ++hist.buckets[2][toByte(p.b)];
        const float lum = 0.299f * p.r + 0.587f * p.g + 0.114f * p.b;
        ++hist.buckets[3][toByte(lum)];
        ++hist.total;
      }
    }
  }
  return hist;
}

}  // namespace tuxels
