#pragma once

#include <array>
#include <cmath>

namespace tuxels {

// 2D affine transform stored as a 2x3 row-major matrix with implicit third
// row [0 0 1]:
//   | a b tx |
//   | c d ty |
// Applied as M(p) = (a*p.x + b*p.y + tx, c*p.x + d*p.y + ty).
struct Affine2D {
  std::array<std::array<float, 3>, 2> m = {{
      {1.f, 0.f, 0.f},
      {0.f, 1.f, 0.f},
  }};

  static Affine2D identity() { return Affine2D{}; }

  static Affine2D translation(float dx, float dy) {
    Affine2D a;
    a.m[0][2] = dx;
    a.m[1][2] = dy;
    return a;
  }

  static Affine2D scaling(float sx, float sy) {
    Affine2D a;
    a.m[0][0] = sx;
    a.m[1][1] = sy;
    return a;
  }

  static Affine2D rotation(float radians) {
    const float c = std::cos(radians), s = std::sin(radians);
    Affine2D a;
    a.m[0][0] = c;
    a.m[0][1] = -s;
    a.m[1][0] = s;
    a.m[1][1] = c;
    return a;
  }

  static Affine2D rotationAround(float radians, float cx, float cy) {
    return translation(-cx, -cy)
        .then(rotation(radians))
        .then(translation(cx, cy));
  }

  static Affine2D scalingAround(float sx, float sy, float cx, float cy) {
    return translation(-cx, -cy)
        .then(scaling(sx, sy))
        .then(translation(cx, cy));
  }

  // Applies `*this` first, then `next`. Matrix: next * this.
  Affine2D then(const Affine2D& next) const {
    const auto& A = m;
    const auto& B = next.m;
    Affine2D r;
    r.m[0][0] = B[0][0] * A[0][0] + B[0][1] * A[1][0];
    r.m[0][1] = B[0][0] * A[0][1] + B[0][1] * A[1][1];
    r.m[0][2] = B[0][0] * A[0][2] + B[0][1] * A[1][2] + B[0][2];
    r.m[1][0] = B[1][0] * A[0][0] + B[1][1] * A[1][0];
    r.m[1][1] = B[1][0] * A[0][1] + B[1][1] * A[1][1];
    r.m[1][2] = B[1][0] * A[0][2] + B[1][1] * A[1][2] + B[1][2];
    return r;
  }

  // Returns the inverse transform, or identity if the transform is singular
  // (|det| below kSingularEps).
  Affine2D inverse() const {
    constexpr float kSingularEps = 1e-9f;
    const float a = m[0][0], b = m[0][1], tx = m[0][2];
    const float c = m[1][0], d = m[1][1], ty = m[1][2];
    const float det = a * d - b * c;
    if (std::fabs(det) < kSingularEps) return identity();
    const float inv = 1.f / det;
    Affine2D r;
    r.m[0][0] = d * inv;
    r.m[0][1] = -b * inv;
    r.m[0][2] = (b * ty - d * tx) * inv;
    r.m[1][0] = -c * inv;
    r.m[1][1] = a * inv;
    r.m[1][2] = (c * tx - a * ty) * inv;
    return r;
  }

  void mapPoint(float x, float y, float& outX, float& outY) const {
    outX = m[0][0] * x + m[0][1] * y + m[0][2];
    outY = m[1][0] * x + m[1][1] * y + m[1][2];
  }
};

}  // namespace tuxels
