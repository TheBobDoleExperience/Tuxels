#include "layers/PixelLayer.h"

namespace tuxels {

bool PixelLayer::renderTile(TileCoord tc, Rgba32F* out) const {
  const Tile* t = image.tiles().find(tc);
  if (!t) return false;
  const Rgba32F* src = t->data();
  bool anyOpaque = false;
  for (int i = 0; i < kTilePixels; ++i) {
    out[i] = src[i];
    if (src[i].a > 0.f) anyOpaque = true;
  }
  return anyOpaque;
}

}  // namespace tuxels
