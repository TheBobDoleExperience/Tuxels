#include "compositor/compose.h"

#include <algorithm>
#include <vector>

#include "compositor/blend.h"
#include "core/Tile.h"
#include "layers/AdjustmentLayer.h"

namespace tuxels {

namespace {

// Render a LayerOverride's image into a tile's local buffer using the same
// overlap math as PixelLayer::renderTile, but skipping masks. Returns true
// if any non-transparent pixel was written.
bool renderOverrideTile(const LayerOverride& ov, TileCoord tc, Rgba32F* out) {
  const int docX0 = tc.tx * kTilePx;
  const int docY0 = tc.ty * kTilePx;
  const int layX0 = docX0 - ov.originX;
  const int layY0 = docY0 - ov.originY;
  const int layW = ov.image->width();
  const int layH = ov.image->height();
  const int ox0 = std::max(0, -layX0);
  const int oy0 = std::max(0, -layY0);
  const int ox1 = std::min(kTilePx, layW - layX0);
  const int oy1 = std::min(kTilePx, layH - layY0);
  if (ox0 >= ox1 || oy0 >= oy1) return false;

  for (int i = 0; i < kTilePixels; ++i) out[i] = Rgba32F::transparent();

  TileRowCursor cur(*ov.image);
  bool anyOpaque = false;
  for (int py = oy0; py < oy1; ++py) {
    const int ly = layY0 + py;
    for (int px = ox0; px < ox1; ++px) {
      const int lx = layX0 + px;
      const Rgba32F c = cur.at(lx, ly);
      if (c.a <= 0.f) continue;
      out[py * kTilePx + px] = c;
      anyOpaque = true;
    }
  }
  return anyOpaque;
}

void composeTileRange(const LayerTree& tree, TuxImage& out, int tx0, int ty0,
                      int tx1, int ty1, const LayerOverride* override) {
  std::vector<Rgba32F> accum(kTilePixels);
  std::vector<Rgba32F> layerTile(kTilePixels);
  std::vector<Rgba32F> adjScratch(kTilePixels);
  // Per-tile capture of the most recent non-clipped pixel layer's source
  // alpha. M4-S1: clipped adjustments multiply their lerp factor by this
  // so the effect is gated by the base layer's alpha. `hasBase` becomes
  // true the first time a pixel layer renders into the tile; resets per
  // tile (next outer iteration).
  std::vector<float> lastBaseAlpha(kTilePixels);

  // M5-S0 stub: flatten the tree once before the per-tile loop. Until S1
  // wires real recursion, groups are walked as if Pass-Through (children
  // appear in the flattened list; the group node itself is currently
  // ignored by the per-layer dispatch since it returns false from
  // `renderTile` and is not an Adjustment).
  const std::vector<const LayerBase*> flat = tree.flatten();

  for (int ty = ty0; ty < ty1; ++ty) {
    for (int tx = tx0; tx < tx1; ++tx) {
      const TileCoord tc{tx, ty};
      std::fill(accum.begin(), accum.end(), Rgba32F::transparent());
      bool hasBase = false;

      for (const LayerBase* layer : flat) {
        if (!layer->visible || layer->opacity <= 0.f) continue;
        // Group nodes contribute no pixels of their own — children have
        // already been emitted before the group via flatten()'s child-then-
        // self order. Skip until S1's compose recursion replaces this.
        if (layer->kind() == LayerKind::Group) continue;

        if (layer->kind() == LayerKind::Adjustment) {
          // Transform-in-place path: copy the current accumulator into a
          // scratch buffer, let the adjustment rewrite it, then lerp the
          // transformed value back over the original using
          // `factor = mask.r * opacity`. Transparent accum pixels are
          // skipped — an empty region stays empty. Mask tile alignment
          // matches compose tile alignment (adjustment layers have
          // origin (0,0) and a doc-sized mask), so one tile lookup
          // covers the whole inner loop.
          //
          // M4-S1 clipping mask path: when `clipToBelow` is set and a base
          // layer has been seen for this tile, multiply the lerp factor
          // by `lastBaseAlpha[idx]` so the adjustment is confined to the
          // base layer's opaque region. With no preceding base, the
          // clipped adjustment is a no-op (matches PS).
          const auto* adj = static_cast<const AdjustmentLayer*>(layer);
          if (layer->clipToBelow && !hasBase) continue;

          adjScratch = accum;
          adj->applyToAccum(tc, adjScratch.data());

          const bool hasMask = layer->mask && layer->mask->enabled;
          const Tile* maskTile =
              hasMask ? layer->mask->image.tiles().find(tc) : nullptr;
          const float opacity = layer->opacity;
          const bool clipped = layer->clipToBelow;

          for (int py = 0; py < kTilePx; ++py) {
            for (int px = 0; px < kTilePx; ++px) {
              const int idx = py * kTilePx + px;
              if (accum[idx].a <= 0.f) continue;
              const float maskV = maskTile ? maskTile->at(px, py).r : 1.f;
              float f = opacity * maskV;
              if (clipped) f *= lastBaseAlpha[idx];
              if (f <= 0.f) continue;
              const Rgba32F a = accum[idx];
              const Rgba32F b = adjScratch[idx];
              const float inv = 1.f - f;
              accum[idx] = {a.r * inv + b.r * f, a.g * inv + b.g * f,
                            a.b * inv + b.b * f, a.a * inv + b.a * f};
            }
          }
          // Adjustment layers do NOT update lastBaseAlpha — they're
          // transformations of the composite, not base content. The next
          // clipped adjustment in the stack still gates against the same
          // pixel layer below.
          continue;
        }

        std::fill(layerTile.begin(), layerTile.end(), Rgba32F::transparent());
        const bool useOverride = override && override->image &&
                                 override->layerId == layer->id;
        const bool rendered =
            useOverride ? renderOverrideTile(*override, tc, layerTile.data())
                        : layer->renderTile(tc, layerTile.data());
        if (!rendered) continue;

        // renderTile already translated by the layer's origin and pre-
        // multiplied any enabled mask into the source alpha. compose only
        // sees doc-coord tiles with effective-alpha pixels from here on.
        const uint32_t seed = layer->noiseSeed();
        const BlendMode mode = layer->blend;
        const float opacity = layer->opacity;
        if (opacity <= 0.f) continue;

        // Capture this layer's source alpha for any clipped adjustments
        // immediately above it. Done before blending so we record the
        // layer's own contribution alpha, not the post-blend accumulator
        // alpha (which would also include layers below).
        for (int i = 0; i < kTilePixels; ++i) {
          lastBaseAlpha[i] = layerTile[i].a;
        }
        hasBase = true;

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

void compose(const LayerTree& tree, TuxImage& out,
             const LayerOverride* override) {
  if (out.width() <= 0 || out.height() <= 0) return;
  const Rect tb = out.tileBounds();
  composeTileRange(tree, out, tb.x, tb.y, tb.x + tb.w, tb.y + tb.h, override);
}

void compose(const LayerTree& tree, TuxImage& out, Rect pixelRect,
             const LayerOverride* override) {
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

  composeTileRange(tree, out, tx0, ty0, tx1, ty1, override);
}

}  // namespace tuxels
