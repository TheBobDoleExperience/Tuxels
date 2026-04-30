#include "compositor/compose.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "compositor/blend.h"
#include "core/Tile.h"
#include "layers/AdjustmentLayer.h"
#include "layers/GroupLayer.h"

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

// Per-tile context — scratch buffers shared across the recursion (each layer
// fully consumes them within its own iteration before the next layer / group
// recursion begins, so a single allocation per outer call is safe).
struct Ctx {
  TileCoord tc;
  Rgba32F* layerTile;     // scratch, kTilePixels
  Rgba32F* adjScratch;    // scratch, kTilePixels
  std::span<const LayerOverride> overrides;
};

// Find the override matching `layerId` in the span, or nullptr.
inline const LayerOverride* findOverride(std::span<const LayerOverride> overrides,
                                          LayerId id) {
  for (const auto& ov : overrides) {
    if (ov.layerId == id && ov.image) return &ov;
  }
  return nullptr;
}

// Recursively composite a list of children into `accum` for one tile.
// `lastBaseAlpha[kTilePixels]` and `hasBase` track the per-pixel alpha of
// the most recent non-clipped pixel layer for the purposes of clipping
// adjustments / clipped groups. Pass-Through groups share the parent's
// (accum, lastBaseAlpha, hasBase); isolated groups allocate their own.
void composeChildren(const Ctx& ctx,
                     const std::vector<std::unique_ptr<LayerBase>>& layers,
                     Rgba32F* accum, float* lastBaseAlpha, bool& hasBase);

// Pass-Through path. Children share the parent scope. When opacity == 1,
// no mask, and no clipToBelow we inline the recursion (bit-exact with
// flat). Otherwise we snapshot accum, recurse, then lerp pre vs post by
// `f = opacity * maskV` per pixel.
void composeGroupPassThrough(const Ctx& ctx, const GroupLayer& g,
                              Rgba32F* accum, float* lastBaseAlpha,
                              bool& hasBase) {
  const bool hasMask = g.mask && g.mask->enabled;
  const Tile* maskTile =
      hasMask ? g.mask->image.tiles().find(ctx.tc) : nullptr;
  const float opacity = g.opacity;
  const bool clipped = g.clipToBelow;

  // Clipped Pass-Through group with no base below = no-op. Skip the
  // recursion entirely (matches PS: a clipping mask with nothing to clip to
  // is empty). Children inside don't run, so they don't establish hasBase
  // for layers above either — same as the per-adjustment clipped branch.
  if (clipped && !hasBase) return;

  // Hot path: opacity 1, no mask, no clip → inline recursion, byte-exact
  // with a flat layer list.
  const bool nonTrivial = (opacity < 1.f) || hasMask || clipped;
  if (!nonTrivial) {
    composeChildren(ctx, g.children, accum, lastBaseAlpha, hasBase);
    return;
  }

  std::vector<Rgba32F> accumBefore(accum, accum + kTilePixels);
  composeChildren(ctx, g.children, accum, lastBaseAlpha, hasBase);

  for (int py = 0; py < kTilePx; ++py) {
    for (int px = 0; px < kTilePx; ++px) {
      const int idx = py * kTilePx + px;
      const float maskV = maskTile ? maskTile->at(px, py).r : 1.f;
      float f = opacity * maskV;
      if (clipped) f *= lastBaseAlpha[idx];
      if (f >= 1.f) continue;            // post wins entirely — accum unchanged
      if (f <= 0.f) {                     // pre wins entirely — restore snapshot
        accum[idx] = accumBefore[idx];
        continue;
      }
      const Rgba32F a = accumBefore[idx];
      const Rgba32F b = accum[idx];
      const float inv = 1.f - f;
      accum[idx] = {a.r * inv + b.r * f, a.g * inv + b.g * f,
                    a.b * inv + b.b * f, a.a * inv + b.a * f};
    }
  }
}

// Isolated path. Children composite into a private accumulator with their
// own scope-local lastBaseAlpha + hasBase; the resulting buffer is then
// composited back into the parent via the group's blend mode + opacity +
// mask. Clipped groups gate by the parent's lastBaseAlpha.
void composeGroupIsolated(const Ctx& ctx, const GroupLayer& g,
                           Rgba32F* accum, float* lastBaseAlpha,
                           bool& hasBase) {
  const bool hasMask = g.mask && g.mask->enabled;
  const Tile* maskTile =
      hasMask ? g.mask->image.tiles().find(ctx.tc) : nullptr;
  const float opacity = g.opacity;
  const bool clipped = g.clipToBelow;
  if (clipped && !hasBase) return;  // no base → group is a no-op

  std::vector<Rgba32F> accum2(kTilePixels, Rgba32F::transparent());
  std::vector<float> lastBaseAlpha2(kTilePixels, 0.f);
  bool hasBase2 = false;
  composeChildren(ctx, g.children, accum2.data(), lastBaseAlpha2.data(),
                  hasBase2);

  if (!hasBase2) return;  // group produced nothing — skip back-composite

  const uint32_t seed = g.noiseSeed();
  const BlendMode mode = g.blend;

  for (int py = 0; py < kTilePx; ++py) {
    for (int px = 0; px < kTilePx; ++px) {
      const int idx = py * kTilePx + px;
      const Rgba32F src = accum2[idx];
      if (src.a <= 0.f) continue;
      const float maskV = maskTile ? maskTile->at(px, py).r : 1.f;
      float f = opacity * maskV;
      if (clipped) f *= lastBaseAlpha[idx];
      if (f <= 0.f) continue;
      const int absX = ctx.tc.tx * kTilePx + px;
      const int absY = ctx.tc.ty * kTilePx + py;
      accum[idx] =
          compositePixel(accum[idx], src, mode, f, seed, absX, absY);
    }
  }

  // Update parent's hasBase + per-pixel base alpha so a clipped adjustment
  // immediately above this group gates against the group's contribution
  // (matching the per-pixel-layer base capture). Use accum2 alpha (the
  // group's pre-back-composite output) so the captured base reflects the
  // group's own contribution rather than the post-composite mix.
  hasBase = true;
  for (int i = 0; i < kTilePixels; ++i) {
    lastBaseAlpha[i] = accum2[i].a;
  }
}

void composeChildren(const Ctx& ctx,
                     const std::vector<std::unique_ptr<LayerBase>>& layers,
                     Rgba32F* accum, float* lastBaseAlpha, bool& hasBase) {
  for (const auto& up : layers) {
    const LayerBase* layer = up.get();
    if (!layer) continue;
    if (!layer->visible || layer->opacity <= 0.f) continue;

    if (layer->kind() == LayerKind::Group) {
      const auto* g = static_cast<const GroupLayer*>(layer);
      if (g->blend == BlendMode::PassThrough) {
        composeGroupPassThrough(ctx, *g, accum, lastBaseAlpha, hasBase);
      } else {
        composeGroupIsolated(ctx, *g, accum, lastBaseAlpha, hasBase);
      }
      continue;
    }

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

      for (int i = 0; i < kTilePixels; ++i) ctx.adjScratch[i] = accum[i];
      adj->applyToAccum(ctx.tc, ctx.adjScratch);

      const bool hasMask = layer->mask && layer->mask->enabled;
      const Tile* maskTile =
          hasMask ? layer->mask->image.tiles().find(ctx.tc) : nullptr;
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
          const Rgba32F b = ctx.adjScratch[idx];
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

    // Pixel layer.
    for (int i = 0; i < kTilePixels; ++i) {
      ctx.layerTile[i] = Rgba32F::transparent();
    }
    const LayerOverride* ov = findOverride(ctx.overrides, layer->id);
    const bool rendered =
        ov ? renderOverrideTile(*ov, ctx.tc, ctx.layerTile)
           : layer->renderTile(ctx.tc, ctx.layerTile);
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
      lastBaseAlpha[i] = ctx.layerTile[i].a;
    }
    hasBase = true;

    for (int py = 0; py < kTilePx; ++py) {
      for (int px = 0; px < kTilePx; ++px) {
        const int idx = py * kTilePx + px;
        const Rgba32F src = ctx.layerTile[idx];
        if (src.a <= 0.f) continue;
        const int absX = ctx.tc.tx * kTilePx + px;
        const int absY = ctx.tc.ty * kTilePx + py;
        accum[idx] =
            compositePixel(accum[idx], src, mode, opacity, seed, absX, absY);
      }
    }
  }
}

void composeTileRange(const LayerTree& tree, TuxImage& out, int tx0, int ty0,
                      int tx1, int ty1,
                      std::span<const LayerOverride> overrides) {
  std::vector<Rgba32F> accum(kTilePixels);
  std::vector<Rgba32F> layerTile(kTilePixels);
  std::vector<Rgba32F> adjScratch(kTilePixels);
  // Per-tile capture of the most recent non-clipped pixel layer's source
  // alpha. M4-S1: clipped adjustments multiply their lerp factor by this
  // so the effect is gated by the base layer's alpha. M5-S1: groups also
  // consume this (clipped groups gate by the parent's lastBaseAlpha) and
  // produce it (an isolated group's accum2 alpha becomes the new base for
  // any clipped adjustment immediately above the group).
  std::vector<float> lastBaseAlpha(kTilePixels);

  const auto& root = tree.raw();

  for (int ty = ty0; ty < ty1; ++ty) {
    for (int tx = tx0; tx < tx1; ++tx) {
      const TileCoord tc{tx, ty};
      std::fill(accum.begin(), accum.end(), Rgba32F::transparent());
      bool hasBase = false;

      Ctx ctx{tc, layerTile.data(), adjScratch.data(), overrides};
      composeChildren(ctx, root, accum.data(), lastBaseAlpha.data(), hasBase);

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
             std::span<const LayerOverride> overrides) {
  if (out.width() <= 0 || out.height() <= 0) return;
  const Rect tb = out.tileBounds();
  composeTileRange(tree, out, tb.x, tb.y, tb.x + tb.w, tb.y + tb.h, overrides);
}

void compose(const LayerTree& tree, TuxImage& out, Rect pixelRect,
             std::span<const LayerOverride> overrides) {
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

  composeTileRange(tree, out, tx0, ty0, tx1, ty1, overrides);
}

// Single-override convenience wrappers — wrap the pointer in a 0/1-element
// span and forward. Tests + non-Transform code paths still pass nullptr
// or an explicit pointer.
void compose(const LayerTree& tree, TuxImage& out,
             const LayerOverride* override) {
  if (override) {
    compose(tree, out, std::span<const LayerOverride>(override, 1));
  } else {
    compose(tree, out, std::span<const LayerOverride>{});
  }
}

void compose(const LayerTree& tree, TuxImage& out, Rect pixelRect,
             const LayerOverride* override) {
  if (override) {
    compose(tree, out, pixelRect, std::span<const LayerOverride>(override, 1));
  } else {
    compose(tree, out, pixelRect, std::span<const LayerOverride>{});
  }
}

}  // namespace tuxels
