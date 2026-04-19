#include "compositor/compose.h"

#include <algorithm>
#include <vector>

#include "compositor/blend.h"
#include "core/Tile.h"

namespace tuxels {

void compose(const LayerTree& tree, TuxImage& out) {
  if (out.width() <= 0 || out.height() <= 0) return;
  const Rect tb = out.tileBounds();

  std::vector<Rgba32F> accum(kTilePixels);
  std::vector<Rgba32F> layerTile(kTilePixels);

  for (int ty = tb.y; ty < tb.y + tb.h; ++ty) {
    for (int tx = tb.x; tx < tb.x + tb.w; ++tx) {
      const TileCoord tc{tx, ty};
      std::fill(accum.begin(), accum.end(), Rgba32F::transparent());

      for (std::size_t li = 0; li < tree.size(); ++li) {
        const LayerBase* layer = tree.at(li);
        if (!layer->visible || layer->opacity <= 0.f) continue;

        std::fill(layerTile.begin(), layerTile.end(), Rgba32F::transparent());
        if (!layer->renderTile(tc, layerTile.data())) continue;

        const bool hasMask = layer->mask && layer->mask->enabled;
        const Tile* maskTile =
            hasMask ? layer->mask->image.tiles().find(tc) : nullptr;

        const uint32_t seed = layer->noiseSeed();
        const BlendMode mode = layer->blend;
        const float opacity = layer->opacity;

        for (int py = 0; py < kTilePx; ++py) {
          for (int px = 0; px < kTilePx; ++px) {
            const int idx = py * kTilePx + px;
            const Rgba32F src = layerTile[idx];
            if (src.a <= 0.f) continue;

            float maskV = 1.f;
            if (hasMask) {
              // Absent mask tile defaults to 1.0 (full reveal).
              maskV = maskTile ? maskTile->data()[idx].r : 1.f;
            }
            const float alphaFactor = opacity * maskV;
            if (alphaFactor <= 0.f) continue;

            const int absX = tx * kTilePx + px;
            const int absY = ty * kTilePx + py;
            accum[idx] = compositePixel(accum[idx], src, mode, alphaFactor,
                                        seed, absX, absY);
          }
        }
      }

      const Rect rect = out.pixelRectForTile(tc);
      if (rect.w <= 0 || rect.h <= 0) continue;
      Tile* destTile = out.tiles().getOrCreate(tc);
      for (int py = 0; py < kTilePx; ++py) {
        for (int px = 0; px < kTilePx; ++px) {
          const int idx = py * kTilePx + px;
          destTile->data()[idx] =
              (px < rect.w && py < rect.h) ? accum[idx] : Rgba32F::transparent();
        }
      }
    }
  }
}

}  // namespace tuxels
