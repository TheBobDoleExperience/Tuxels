#pragma once

#include "core/TuxImage.h"

namespace tuxels {

// Raster layer mask. In M0 we reuse the RGBA TuxImage and read only the .r
// channel as grayscale mask intensity. Absent tiles default to 1.0 (reveal).
// Future: single-channel TuxImage variant for memory efficiency.
struct LayerMask {
  TuxImage image;
  bool enabled = true;

  LayerMask() = default;
  LayerMask(int w, int h) : image(w, h) {}
};

}  // namespace tuxels
