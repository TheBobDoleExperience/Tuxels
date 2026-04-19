#pragma once

#include <algorithm>
#include <cmath>

namespace tuxels {

struct Rgba32F {
  float r = 0.f;
  float g = 0.f;
  float b = 0.f;
  float a = 0.f;

  constexpr Rgba32F() = default;
  constexpr Rgba32F(float r_, float g_, float b_, float a_ = 1.f)
      : r(r_), g(g_), b(b_), a(a_) {}

  friend constexpr bool operator==(const Rgba32F&, const Rgba32F&) = default;

  Rgba32F clamped() const {
    return {std::clamp(r, 0.f, 1.f), std::clamp(g, 0.f, 1.f),
            std::clamp(b, 0.f, 1.f), std::clamp(a, 0.f, 1.f)};
  }

  static constexpr Rgba32F transparent() { return {0.f, 0.f, 0.f, 0.f}; }
  static constexpr Rgba32F black()       { return {0.f, 0.f, 0.f, 1.f}; }
  static constexpr Rgba32F white()       { return {1.f, 1.f, 1.f, 1.f}; }
};

inline bool approxEqual(const Rgba32F& a, const Rgba32F& b, float eps = 1e-5f) {
  return std::fabs(a.r - b.r) <= eps && std::fabs(a.g - b.g) <= eps &&
         std::fabs(a.b - b.b) <= eps && std::fabs(a.a - b.a) <= eps;
}

}  // namespace tuxels
