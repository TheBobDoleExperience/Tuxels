#pragma once

#include "core/TuxImage.h"
#include "layers/LayerTree.h"

namespace tuxels {

// Composite all layers in the tree into `out`. `out` must already have its
// width/height set. Tiles are allocated/overwritten for every tile that
// intersects the image bounds (flat image, not sparse).
void compose(const LayerTree& tree, TuxImage& out);

// Recompose only the tiles intersecting `pixelRect` (image-space). Used on
// the paint hot path so a small brush stamp doesn't force a whole-image
// composite.
void compose(const LayerTree& tree, TuxImage& out, Rect pixelRect);

}  // namespace tuxels
