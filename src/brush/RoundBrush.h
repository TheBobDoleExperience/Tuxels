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
  void setColor(Rgba32F c);

  int diameter() const noexcept { return p_.diameter; }
  // Kernel value at local pixel (lx,ly), where 0 ≤ lx,ly < diameter.
  float kernel(int lx, int ly) const;
  // Integer stamp spacing in image pixels, always ≥ 1.
  int spacingPx() const noexcept;

 private:
  void rebuildKernel();
  RoundBrushParams p_;
  std::vector<float> kernel_;  // row-major, diameter × diameter
};

}  // namespace tuxels
