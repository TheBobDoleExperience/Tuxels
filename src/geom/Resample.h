#pragma once

#include "core/TuxImage.h"
#include "geom/Affine2D.h"

namespace tuxels {

// Bilinear-sample `src` into `dst` using the given `dstToSrc` affine
// (maps a dst pixel coordinate into the source's coordinate frame). Uses
// pixel-center sampling convention — pixel (i, j) is centered at
// (i + 0.5, j + 0.5). Samples outside `src` contribute (0, 0, 0, 0).
//
// Blending uses premultiplied-alpha bilerp + un-premultiply on read-out so
// RGB channels of transparent neighbors don't bleed through.
//
// `dst` must already be sized (caller owns bounds). Fully-transparent
// results are skipped via setPixel to avoid creating empty tiles.
void resampleBilinear(const TuxImage& src, TuxImage& dst,
                      const Affine2D& dstToSrc);

}  // namespace tuxels
