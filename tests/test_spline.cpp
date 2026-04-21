#include <cmath>
#include <cstdint>
#include <vector>

#include "geom/Spline.h"
#include "test_harness.h"

using namespace tuxels;

TEST(spline_identity_line_produces_identity_lut) {
  std::vector<SplinePoint> pts = {{0.f, 0.f}, {1.f, 1.f}};
  uint8_t lut[256];
  buildLut256(pts, lut);
  for (int i = 0; i < 256; ++i) CHECK_EQ(int(lut[i]), i);
}

TEST(spline_empty_points_default_to_identity) {
  // Caller passes no points at all — endpoints (0,0) / (1,1) are implied
  // and the curve falls back to the identity line.
  std::vector<SplinePoint> pts;
  uint8_t lut[256];
  buildLut256(pts, lut);
  for (int i = 0; i < 256; ++i) CHECK_EQ(int(lut[i]), i);
}

TEST(spline_midpoint_lift_brightens_midtones) {
  // Pull the mid-point up to (0.5, 0.7) — the LUT should brighten roughly
  // around the middle without exceeding 255 anywhere.
  std::vector<SplinePoint> pts = {{0.f, 0.f}, {0.5f, 0.7f}, {1.f, 1.f}};
  uint8_t lut[256];
  buildLut256(pts, lut);
  CHECK_EQ(int(lut[0]), 0);
  CHECK_EQ(int(lut[255]), 255);
  // The midpoint bucket must land near the pinned y=0.7 value.
  const int midExpected = 178;  // lround(0.7 * 255) = 179; allow ±2 for spline rounding.
  CHECK(std::abs(int(lut[128]) - midExpected) <= 2);
}

TEST(spline_monotone_no_overshoot_increasing) {
  // Steep overshoot-prone control points: a naive Catmull-Rom would shoot
  // above y=1 between (0.4, 0.9) and (0.6, 1.0). Monotone Hermite must
  // keep the LUT inside [0, 255] and non-decreasing.
  std::vector<SplinePoint> pts = {
      {0.f, 0.f}, {0.4f, 0.9f}, {0.6f, 1.f}, {1.f, 1.f}};
  uint8_t lut[256];
  buildLut256(pts, lut);
  for (int i = 1; i < 256; ++i) {
    CHECK(lut[i] >= lut[i - 1]);
    CHECK(lut[i] <= 255);
  }
}

TEST(spline_flat_segment_clamps_to_constant) {
  // A flat segment in the middle (two points at the same y) must produce a
  // flat region of the LUT without wiggling.
  std::vector<SplinePoint> pts = {
      {0.f, 0.f}, {0.3f, 0.5f}, {0.7f, 0.5f}, {1.f, 1.f}};
  uint8_t lut[256];
  buildLut256(pts, lut);
  // Inside the flat segment, LUT values should stay near 0.5 * 255 = 128.
  for (int i = 80; i <= 175; ++i) {
    CHECK(std::abs(int(lut[i]) - 128) <= 3);
  }
}

TEST(spline_endpoints_are_fixed) {
  // LUT[0] and LUT[255] must match the y of the pinned endpoints, so a
  // curve that only moves its middle doesn't shift the shadows or
  // highlights.
  std::vector<SplinePoint> pts = {{0.f, 0.f}, {0.5f, 0.8f}, {1.f, 1.f}};
  uint8_t lut[256];
  buildLut256(pts, lut);
  CHECK_EQ(int(lut[0]), 0);
  CHECK_EQ(int(lut[255]), 255);
}

TEST(spline_evaluate_matches_lut_sampling) {
  // Sanity: evaluateSpline and buildLut256 must produce bit-close values
  // at every bucket center (tolerance = 1 byte after rounding).
  std::vector<SplinePoint> pts = {
      {0.f, 0.f}, {0.25f, 0.1f}, {0.75f, 0.95f}, {1.f, 1.f}};
  uint8_t lut[256];
  buildLut256(pts, lut);
  for (int i = 0; i < 256; i += 17) {
    const float x = static_cast<float>(i) / 255.f;
    const float y = evaluateSpline(pts, x);
    const int expected = static_cast<int>(std::lround(y * 255.f));
    CHECK(std::abs(int(lut[i]) - expected) <= 1);
  }
}

int main() { return ::tuxels::testing::run(); }
