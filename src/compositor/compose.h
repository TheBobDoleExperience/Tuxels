#pragma once

#include <span>

#include "core/TuxImage.h"
#include "layers/LayerBase.h"
#include "layers/LayerTree.h"

namespace tuxels {

// Preview-time substitute for a single layer. When compose encounters a
// layer whose id matches `layerId`, it renders `image` at (originX,
// originY) in place of the real layer's pixels. Used by the Transform tool
// to show a live resample without mutating the committed layer. Masks are
// not applied for overrides — the tool is expected to pre-bake any mask
// effect into `image` when relevant.
//
// M10-S0: compose() accepts a span of overrides so multiple layers can be
// substituted at once (multi-select Free Transform). Each layer's id is
// looked up against the span; first match wins.
struct LayerOverride {
  LayerId layerId = 0;
  const TuxImage* image = nullptr;
  int originX = 0;
  int originY = 0;
};

// --- Single-override convenience overloads (legacy callers + tests). ---

// Composite all layers in the tree into `out`. `out` must already have its
// width/height set. Tiles are allocated/overwritten for every tile that
// intersects the image bounds (flat image, not sparse).
void compose(const LayerTree& tree, TuxImage& out,
             const LayerOverride* override = nullptr);

// Recompose only the tiles intersecting `pixelRect` (image-space). Used on
// the paint hot path so a small brush stamp doesn't force a whole-image
// composite.
void compose(const LayerTree& tree, TuxImage& out, Rect pixelRect,
             const LayerOverride* override = nullptr);

// --- Multi-override variants (M10-S0). ---

void compose(const LayerTree& tree, TuxImage& out,
             std::span<const LayerOverride> overrides);
void compose(const LayerTree& tree, TuxImage& out, Rect pixelRect,
             std::span<const LayerOverride> overrides);

}  // namespace tuxels
