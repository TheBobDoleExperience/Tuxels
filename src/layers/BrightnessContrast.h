#pragma once

#include <cstdint>

#include "layers/AdjustmentLayer.h"

namespace tuxels {

// Linear brightness + contrast. `brightness` is an additive offset,
// `contrast` is a multiplicative stretch around the 0.5 midpoint. Identity
// is {0, 0}. Both clamp to [-1, 1] in the UI; the LUT itself clamps each
// output entry to [0, 1], so out-of-range inputs saturate cleanly.
struct BrightnessContrastParams {
  float brightness = 0.f;
  float contrast = 0.f;
};

class BrightnessContrast : public AdjustmentLayer {
 public:
  BrightnessContrast();

  void setParams(const BrightnessContrastParams& p);
  const BrightnessContrastParams& params() const { return params_; }

  void applyToAccum(TileCoord tc, Rgba32F* accum) const override;

  // Direct LUT access for tests.
  const uint8_t* lut() const { return lut_; }

 private:
  void rebuildLut();

  BrightnessContrastParams params_{};
  uint8_t lut_[256]{};
};

}  // namespace tuxels
