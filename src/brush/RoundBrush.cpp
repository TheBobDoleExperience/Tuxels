#include "brush/RoundBrush.h"

#include <algorithm>
#include <cmath>

namespace tuxels {

void RoundBrush::setParams(RoundBrushParams p) {
  p.diameter = std::max(1, p.diameter);
  p.hardness = std::clamp(p.hardness, 0.f, 1.f);
  p.opacity = std::clamp(p.opacity, 0.f, 1.f);
  p.flow = std::clamp(p.flow, 0.f, 1.f);
  p.spacingRatio = std::clamp(p.spacingRatio, 0.01f, 1.f);
  p.sizeJitter = std::clamp(p.sizeJitter, 0.f, 1.f);
  p.opacityJitter = std::clamp(p.opacityJitter, 0.f, 1.f);
  p.pressureMultiplier = std::max(0.f, p.pressureMultiplier);
  p_ = p;
  rebuildKernel();
}

void RoundBrush::setDiameter(int d) {
  auto q = p_; q.diameter = d; setParams(q);
}
void RoundBrush::setHardness(float h) {
  auto q = p_; q.hardness = h; setParams(q);
}
void RoundBrush::setOpacity(float o) {
  auto q = p_; q.opacity = o; setParams(q);
}
void RoundBrush::setFlow(float f) {
  auto q = p_; q.flow = f; setParams(q);
}
void RoundBrush::setSizeJitter(float j) {
  auto q = p_; q.sizeJitter = j; setParams(q);
}
void RoundBrush::setOpacityJitter(float j) {
  auto q = p_; q.opacityJitter = j; setParams(q);
}
void RoundBrush::setSpacingRatio(float r) {
  auto q = p_; q.spacingRatio = r; setParams(q);
}
void RoundBrush::setColor(Rgba32F c) { p_.color = c; }

int RoundBrush::spacingPx() const noexcept {
  return std::max(1,
                  static_cast<int>(std::lround(p_.diameter * p_.spacingRatio)));
}

float RoundBrush::kernel(int lx, int ly) const {
  return kernel_[ly * p_.diameter + lx];
}

void RoundBrush::buildKernel(int D, float hardness, std::vector<float>& out) {
  D = std::max(1, D);
  hardness = std::clamp(hardness, 0.f, 1.f);
  out.assign(static_cast<std::size_t>(D) * static_cast<std::size_t>(D), 0.f);
  const float radius = D * 0.5f;
  // Samples are centered on pixel centers so the kernel is symmetric for
  // both odd and even diameters.
  for (int y = 0; y < D; ++y) {
    for (int x = 0; x < D; ++x) {
      const float dx = (x + 0.5f) - radius;
      const float dy = (y + 0.5f) - radius;
      const float d = std::sqrt(dx * dx + dy * dy);
      const float r = d / radius;
      float k;
      if (r >= 1.f) {
        k = 0.f;
      } else if (hardness >= 0.999f || r <= hardness) {
        k = 1.f;
      } else {
        const float t = (r - hardness) / (1.f - hardness);
        k = 1.f - t * t * (3.f - 2.f * t);  // inverse smoothstep
      }
      out[static_cast<std::size_t>(y) * static_cast<std::size_t>(D) + x] = k;
    }
  }
}

void RoundBrush::rebuildKernel() { buildKernel(p_.diameter, p_.hardness, kernel_); }

}  // namespace tuxels
