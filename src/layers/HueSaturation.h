#pragma once

#include "layers/AdjustmentLayer.h"

namespace tuxels {

// Master Hue / Saturation / Lightness adjustment. Per-color-range editing
// (Reds/Yellows/Greens/etc.) is deferred. Identity is {0, 0, 0}.
// `hueShift` is in degrees [-180, 180] and wraps mod 360 at apply time.
// `saturation` and `lightness` are each in [-1, 1]: saturation scales
// `s' = s * (1 + saturation)`; lightness offsets `l' = l + lightness`.
struct HueSaturationParams {
  float hueShift = 0.f;
  float saturation = 0.f;
  float lightness = 0.f;
};

class HueSaturation : public AdjustmentLayer {
 public:
  HueSaturation();

  void setParams(const HueSaturationParams& p);
  const HueSaturationParams& params() const { return params_; }

  void applyToAccum(TileCoord tc, Rgba32F* accum) const override;

 private:
  HueSaturationParams params_{};
};

}  // namespace tuxels
