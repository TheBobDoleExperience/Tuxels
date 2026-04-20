#include "layers/PixelLayer.h"

#include <algorithm>
#include <climits>

namespace tuxels {

bool PixelLayer::renderTile(TileCoord tc, Rgba32F* out) const {
  const int docX0 = tc.tx * kTilePx;
  const int docY0 = tc.ty * kTilePx;
  // Layer-local coord of the tile's (0,0) corner.
  const int layX0 = docX0 - originX;
  const int layY0 = docY0 - originY;
  const int layW = image.width();
  const int layH = image.height();

  // Intersect the tile's kTilePx-square in layer-local coords with
  // [0..layW) × [0..layH). The resulting ox/oy range is the tile-local
  // pixel span to write; pixels outside the overlap stay transparent.
  const int ox0 = std::max(0, -layX0);
  const int oy0 = std::max(0, -layY0);
  const int ox1 = std::min(kTilePx, layW - layX0);
  const int oy1 = std::min(kTilePx, layH - layY0);
  if (ox0 >= ox1 || oy0 >= oy1) return false;

  // Conservative: clear the whole tile so callers don't rely on stale data
  // for pixels outside the overlap. Measured to be cheap relative to the
  // per-pixel sample path below.
  for (int i = 0; i < kTilePixels; ++i) out[i] = Rgba32F::transparent();

  const bool hasMask = mask && mask->enabled;

  // Layer source cursor — one tile-hash per boundary crossing.
  TileRowCursor layerCursor(image);

  // Mask sampling: absent tiles default to 1.0 (reveal), which TileRowCursor
  // would return as 0.0, so we track the current mask tile manually with the
  // same tile-boundary cache and fall back to 1.0 when a tile is absent.
  const TileStore* maskTiles = hasMask ? &mask->image.tiles() : nullptr;
  int maskCurTx = INT_MIN, maskCurTy = INT_MIN;
  const Tile* maskTile = nullptr;

  bool anyOpaque = false;
  for (int py = oy0; py < oy1; ++py) {
    const int ly = layY0 + py;
    for (int px = ox0; px < ox1; ++px) {
      const int lx = layX0 + px;
      const Rgba32F c = layerCursor.at(lx, ly);
      if (c.a <= 0.f) continue;

      float maskV = 1.f;
      if (hasMask) {
        const int mtx = lx / kTilePx;
        const int mty = ly / kTilePx;
        if (mtx != maskCurTx || mty != maskCurTy) {
          maskCurTx = mtx;
          maskCurTy = mty;
          maskTile = maskTiles->find({mtx, mty});
        }
        if (maskTile) {
          maskV = maskTile->at(lx - mtx * kTilePx, ly - mty * kTilePx).r;
        }
      }
      if (maskV <= 0.f) continue;

      const int idx = py * kTilePx + px;
      out[idx] = {c.r, c.g, c.b, c.a * maskV};
      anyOpaque = true;
    }
  }
  return anyOpaque;
}

}  // namespace tuxels
