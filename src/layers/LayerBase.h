#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "compositor/BlendMode.h"
#include "core/Pixel.h"
#include "core/Tile.h"
#include "layers/LayerColorLabel.h"
#include "layers/LayerMask.h"

namespace tuxels {

using LayerId = uint64_t;

// Discriminator used by the compositor to pick a render path. Pixel layers
// contribute their own tile pixels (blended over the accumulator); adjustment
// layers transform the accumulator in place. Group layers (M5) contain other
// layers and are dispatched via the compose recursion (their `renderTile`
// returns false). See `AdjustmentLayer`, `GroupLayer`, and `compose()` for
// the corresponding dispatch.
enum class LayerKind { Pixel, Adjustment, Group };

class LayerBase {
 public:
  virtual ~LayerBase() = default;

  LayerId id = 0;
  std::string name;
  bool visible = true;
  float opacity = 1.f;
  BlendMode blend = BlendMode::Normal;
  std::unique_ptr<LayerMask> mask;

  // Doc-coord of the layer's (0, 0) pixel. Added M2-S0 so layers can live
  // anywhere on the document grid (Place Image, Move, Free Transform) without
  // forcing the layer's backing image to be doc-sized. Masks share their
  // owning layer's origin and dimensions. Zero origin behaves identically to
  // the pre-origin code path, keeping M1 blank-layers-at-(0,0) untouched.
  int originX = 0;
  int originY = 0;

  // PS "Create Clipping Mask" flag (M4-S1). When true and this layer is an
  // adjustment, compose gates the adjustment's effect by the per-pixel alpha
  // of the most recent non-clipped pixel layer below — so the adjustment
  // only modifies pixels where the base layer below is opaque. Multiple
  // contiguous clipped layers share the same base. Clipped adjustment with
  // no preceding pixel layer = no-op (matches PS).
  // Field lives on the base so a future PixelLayer-clipping path can reuse
  // it without surgery, but M4 only honors the flag for adjustment layers.
  bool clipToBelow = false;

  // PS-style color tag (M8-S0). Visual organizer only — does NOT affect
  // compose. Persists in `.txl` v6+. Pre-v6 files load with `None`.
  LayerColorLabel colorLabel = LayerColorLabel::None;

  // Tells the compositor which rendering path applies. `PixelLayer` uses the
  // default; `AdjustmentLayer` overrides to `Adjustment`.
  virtual LayerKind kind() const { return LayerKind::Pixel; }

  // Render this layer's contribution for one tile. Writes kTilePx*kTilePx
  // Rgba32F values into `out`. Returns true if any non-transparent pixel was
  // written; false means the tile is empty and the compositor can skip it.
  //
  // M2-S0: output is in doc-coord tile space. Implementations translate
  // doc→layer coords via `originX/originY`, and fold any enabled mask into
  // the output alpha (absent mask tiles default to 1.0 = reveal). This lets
  // `compose()` stay origin-agnostic.
  //
  // Adjustment layers override to return `false` (no pixel contribution); the
  // compositor dispatches them via `AdjustmentLayer::applyToAccum` instead.
  virtual bool renderTile(TileCoord tc, Rgba32F* out) const = 0;

  // Dissolve noise seed — derived from layer id so patterns are stable.
  virtual uint32_t noiseSeed() const {
    return static_cast<uint32_t>(id * 0x9E3779B97F4A7C15ull);
  }
};

}  // namespace tuxels
