#pragma once

#include "core/Pixel.h"
#include "core/Tile.h"
#include "layers/LayerBase.h"

namespace tuxels {

// Non-destructive layer kind. Reads the composite-below accumulator for a
// tile and writes a transformed value back into a scratch buffer; compose
// then lerps the scratch over the original using `mask.r * opacity` as the
// factor. Subclasses (Levels, Curves, Hue/Sat, B&C) implement
// `applyToAccum` with their per-pixel transform.
//
// Adjustment layers carry no backing image. Masks are doc-sized and owned
// via the existing `LayerBase::mask` slot; `Document::addAdjustmentLayer`
// auto-attaches a white doc-sized mask on create (matches PS).
class AdjustmentLayer : public LayerBase {
 public:
  LayerKind kind() const final { return LayerKind::Adjustment; }

  // Adjustment layers contribute no pixels of their own — compose
  // dispatches on `kind()` and calls `applyToAccum` instead.
  bool renderTile(TileCoord /*tc*/, Rgba32F* /*out*/) const final {
    return false;
  }

  // Transform the tile's accumulator buffer in place. `accum` on entry is
  // the pre-adjustment composite of all layers below; on return it should
  // hold the modified value. Compose then lerps between the two using the
  // layer's opacity and mask. Must be deterministic — called once per
  // visible tile per recomposite.
  virtual void applyToAccum(TileCoord tc, Rgba32F* accum) const = 0;
};

}  // namespace tuxels
