#include "compositor/compose.h"

#include <algorithm>
#include <vector>

#include "compositor/blend.h"
#include "core/Tile.h"

namespace tuxels {

namespace {

void composeTileRange(const LayerTree& tree, TuxImage& out, int tx0, int ty0,
                      int tx1, int ty1) {
  std::vector<Rgba32F> accum(kTilePixels);
  std::vector<Rgba32F> layerTile(kTilePixels);

  for (int ty = ty0; ty < ty1; ++ty) {
    for (int tx = tx0; tx < tx1; ++tx) {
      const TileCoord tc{tx, ty};
      std::fill(accum.begin(), accum.end(), Rgba32F::transparent());

      for (std::size_t li = 0; li < tree.size(); ++li) {
        const LayerBase* layer = tree.at(li);
        if (!layer->visible || layer->opacity <= 0.f) continue;

        std::fill(layerTile.begin(), layerTile.end(), Rgba32F::transparent());
        if (!layer->renderTile(tc, layerTile.data())) continue;

        // renderTile already translated by the layer's origin and pre-
        // multiplied any enabled mask into the source alpha. compose only
        // sees doc-coord tiles with effective-alpha pixels from here on.
        const uint32_t seed = layer->noiseSeed();
        const BlendMode mode = layer->blend;
        const float opacity = layer->opacity;
        if (opacity <= 0.f) continue;

        for (int py = 0; py < kTilePx; ++py) {
          for (int px = 0; px < kTilePx; ++px) {
            const int idx = py * kTilePx + px;
            const Rgba32F src = layerTile[idx];
            if (src.a <= 0.f) continue;

            const int absX = tx * kTilePx + px;
            const int absY = ty * kTilePx + py;
            accum[idx] = compositePixel(accum[idx], src, mode, opacity,
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

}  // namespace

void compose(const LayerTree& tree, TuxImage& out) {
  if (out.width() <= 0 || out.height() <= 0) return;
  const Rect tb = out.tileBounds();
  composeTileRange(tree, out, tb.x, tb.y, tb.x + tb.w, tb.y + tb.h);
}

void compose(const LayerTree& tree, TuxImage& out, Rect pixelRect) {
  if (out.width() <= 0 || out.height() <= 0) return;
  if (pixelRect.isEmpty()) return;
  const Rect tb = out.tileBounds();

  // Clip to the image's pixel bounds, then map to inclusive tile indices.
  const int px0 = std::max(0, pixelRect.x);
  const int py0 = std::max(0, pixelRect.y);
  const int px1 = std::min(out.width(), pixelRect.right());
  const int py1 = std::min(out.height(), pixelRect.bottom());
  if (px1 <= px0 || py1 <= py0) return;

  auto divFloor = [](int a, int b) {
    return (a / b) - (((a % b) != 0) && ((a ^ b) < 0) ? 1 : 0);
  };
  const int tx0 = std::max(tb.x, divFloor(px0, kTilePx));
  const int ty0 = std::max(tb.y, divFloor(py0, kTilePx));
  const int tx1 = std::min(tb.x + tb.w, divFloor(px1 - 1, kTilePx) + 1);
  const int ty1 = std::min(tb.y + tb.h, divFloor(py1 - 1, kTilePx) + 1);
  if (tx1 <= tx0 || ty1 <= ty0) return;

  composeTileRange(tree, out, tx0, ty0, tx1, ty1);
}

}  // namespace tuxels
