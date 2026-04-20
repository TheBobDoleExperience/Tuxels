#pragma once

#include <vector>

#include "core/Pixel.h"

namespace tuxels {

struct RoundBrushParams {
  int diameter = 20;              // pixels
  float hardness = 0.8f;          // [0,1]; 1 = hard edge, 0 = full gradient
  float opacity = 1.0f;           // [0,1] master tool opacity
  float flow = 1.0f;              // [0,1] per-stamp deposit fraction
  float spacingRatio = 0.1f;      // stamp spacing as fraction of diameter
  // Per-stamp random deviations applied by BrushEngine. Both are symmetric:
  // effective diameter = D * (1 + U(-sizeJitter,+sizeJitter)), effective
  // opacity = op * (1 + U(-opacityJitter,+opacityJitter)) clamped to [0,1].
  // Both default to 0 so jittered code paths are entirely skipped unless a
  // slider has been moved — keeps the no-jitter output bitwise identical.
  float sizeJitter = 0.0f;
  float opacityJitter = 0.0f;
  // Tablet-pressure scale. Unwired in M2 (kept on the params so the tablet
  // integration in M3+ slots in without touching this struct's callers).
  float pressureMultiplier = 1.0f;
  Rgba32F color{0.f, 0.f, 0.f, 1.f};
};

class RoundBrush {
 public:
  explicit RoundBrush(RoundBrushParams p = {}) { setParams(p); }

  const RoundBrushParams& params() const noexcept { return p_; }
  void setParams(RoundBrushParams p);
  void setDiameter(int d);
  void setHardness(float h);
  void setOpacity(float o);
  void setFlow(float f);
  void setSizeJitter(float j);
  void setOpacityJitter(float j);
  void setSpacingRatio(float r);
  void setColor(Rgba32F c);

  int diameter() const noexcept { return p_.diameter; }
  // Kernel value at local pixel (lx,ly), where 0 ≤ lx,ly < diameter.
  float kernel(int lx, int ly) const;
  // Integer stamp spacing in image pixels, always ≥ 1.
  int spacingPx() const noexcept;

  // Fill `out` with a diameter×diameter row-major kernel shaped the way
  // this brush's own kernel is. BrushEngine uses this to build a scratch
  // kernel when size-jitter changes the effective diameter per stamp.
  static void buildKernel(int diameter, float hardness,
                          std::vector<float>& out);

 private:
  void rebuildKernel();
  RoundBrushParams p_;
  std::vector<float> kernel_;  // row-major, diameter × diameter
};

}  // namespace tuxels
