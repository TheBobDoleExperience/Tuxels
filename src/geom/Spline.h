#pragma once

#include <cstdint>
#include <vector>

namespace tuxels {

// A single control point in `[0, 1]²` space. Kept as a local struct (rather
// than QPointF) so `tuxels_core` stays free of Qt dependencies — the dialog
// converts between this and QPointF at the UI seam.
struct SplinePoint {
  float x;
  float y;
};

// Sample a monotone Hermite cubic spline (Fritsch-Carlson variant) defined
// by `pts` at 256 evenly-spaced x values in `[0, 1]` and write a byte LUT
// into `out[0..255]`. Endpoints `(0, 0)` and `(1, 1)` are implied when the
// caller's first / last point doesn't pin them.
//
// Monotone Hermite is the PS "smooth" curve default — it will not overshoot
// the control points, so a user curve that is strictly increasing in y
// produces a strictly non-decreasing LUT. Points are expected sorted by x;
// the caller is responsible for keeping them sorted.
void buildLut256(const std::vector<SplinePoint>& pts, uint8_t* out);

// Evaluate the spline at a single `x` ∈ `[0, 1]`. Useful for tests that want
// to sanity-check a point without building a full LUT.
float evaluateSpline(const std::vector<SplinePoint>& pts, float x);

}  // namespace tuxels
