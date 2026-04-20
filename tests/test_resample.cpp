#include <cmath>

#include "core/Pixel.h"
#include "core/TuxImage.h"
#include "geom/Affine2D.h"
#include "geom/Resample.h"
#include "test_harness.h"

using namespace tuxels;

namespace {

constexpr Rgba32F kRed{1.f, 0.f, 0.f, 1.f};
constexpr Rgba32F kGreen{0.f, 1.f, 0.f, 1.f};
constexpr Rgba32F kTransparent{0.f, 0.f, 0.f, 0.f};
constexpr float kPi = 3.14159265358979323846f;

}  // namespace

TEST(affine_translation_applies_offset) {
  Affine2D t = Affine2D::translation(3.f, -2.f);
  float x, y;
  t.mapPoint(10.f, 4.f, x, y);
  CHECK_NEAR(x, 13.0, 1e-5);
  CHECK_NEAR(y, 2.0, 1e-5);
}

TEST(affine_then_composes_in_sequence_order) {
  // Translate by (5, 0), then scale by 2 — point (1, 1) should land at
  // (12, 2): (1+5, 1) = (6, 1), then *2 = (12, 2).
  Affine2D a =
      Affine2D::translation(5.f, 0.f).then(Affine2D::scaling(2.f, 2.f));
  float x, y;
  a.mapPoint(1.f, 1.f, x, y);
  CHECK_NEAR(x, 12.0, 1e-5);
  CHECK_NEAR(y, 2.0, 1e-5);
}

TEST(affine_inverse_composed_gives_identity) {
  // Build a non-trivial affine and verify M.inverse().then(M) ≈ identity.
  Affine2D m = Affine2D::translation(3.f, -1.f)
                   .then(Affine2D::scaling(2.f, 0.5f))
                   .then(Affine2D::rotation(0.4f));
  Affine2D r = m.inverse().then(m);
  float x, y;
  r.mapPoint(7.f, -4.f, x, y);
  CHECK_NEAR(x, 7.0, 1e-4);
  CHECK_NEAR(y, -4.0, 1e-4);
}

TEST(affine_rotation_around_preserves_pivot) {
  Affine2D r = Affine2D::rotationAround(kPi / 3.f, 4.5f, 7.5f);
  float x, y;
  r.mapPoint(4.5f, 7.5f, x, y);
  CHECK_NEAR(x, 4.5, 1e-5);
  CHECK_NEAR(y, 7.5, 1e-5);
}

TEST(resample_identity_reproduces_source) {
  // Identity transform on a solid image → every pixel equals the source.
  TuxImage src(8, 8);
  src.fill(kRed);
  TuxImage dst(8, 8);
  resampleBilinear(src, dst, Affine2D::identity());
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      const Rgba32F p = dst.getPixel(x, y);
      CHECK_APPROX(p, kRed, 1e-5f);
    }
  }
}

TEST(resample_integer_translation_shifts_pixels_cleanly) {
  // Shift (+2, +1) by making dstToSrc the inverse: samples src at
  // (dx-2, dy-1). Integer shifts + pixel-centered bilerp produce a 1:1
  // copy with offset, so dst interior pixels exactly match src pixels.
  TuxImage src(4, 4);
  src.setPixel(0, 0, kRed);
  src.setPixel(1, 1, kGreen);
  TuxImage dst(8, 8);
  resampleBilinear(src, dst, Affine2D::translation(-2.f, -1.f));
  CHECK_APPROX(dst.getPixel(2, 1), kRed, 1e-5f);
  CHECK_APPROX(dst.getPixel(3, 2), kGreen, 1e-5f);
  // Pixel outside the shifted src footprint should remain empty.
  CHECK_APPROX(dst.getPixel(0, 0), kTransparent, 1e-5f);
  CHECK_APPROX(dst.getPixel(7, 7), kTransparent, 1e-5f);
}

TEST(resample_2x_upscale_fills_interior_and_feathers_edges) {
  // 2x2 solid red upscaled to 4x4. Center 2x2 samples four red pixels
  // and comes out fully red, alpha=1. The outer ring samples partially
  // outside the source; alpha falls off toward the corners.
  TuxImage src(2, 2);
  src.fill(kRed);
  TuxImage dst(4, 4);
  // dstToSrc: dst covers 2x source extent, so scale by 0.5.
  resampleBilinear(src, dst, Affine2D::scaling(0.5f, 0.5f));

  // Interior 2x2 — both axes' t-weights are 0.25 inside valid src.
  for (int y = 1; y <= 2; ++y) {
    for (int x = 1; x <= 2; ++x) {
      const Rgba32F p = dst.getPixel(x, y);
      CHECK_APPROX(p, kRed, 1e-5f);
    }
  }
  // Corner pixel at (0,0): only q11 contributes (the sole in-bounds
  // sample), weight 0.75*0.75 = 0.5625. Red channel stays 1 after
  // un-premult, alpha = 0.5625.
  const Rgba32F corner = dst.getPixel(0, 0);
  CHECK_NEAR(corner.r, 1.0, 1e-5);
  CHECK_NEAR(corner.a, 0.5625, 1e-5);
}

TEST(resample_90deg_rotation_moves_pixel_to_expected_cell) {
  // Rotate a single red pixel at (0, 0) by +90° around the center of a
  // 3x3 image (pivot at (1.5, 1.5)). The red pixel should land at (2, 0).
  TuxImage src(3, 3);
  src.setPixel(0, 0, kRed);
  TuxImage dst(3, 3);
  // We want dst to be src rotated +90° about center. For each dst pixel
  // we sample back into src, so dstToSrc is the inverse (-90° around the
  // same pivot).
  Affine2D dstToSrc = Affine2D::rotationAround(-kPi / 2.f, 1.5f, 1.5f);
  resampleBilinear(src, dst, dstToSrc);
  const Rgba32F moved = dst.getPixel(2, 0);
  CHECK_APPROX(moved, kRed, 1e-4f);
  // The original cell should be transparent in the rotated dst.
  const Rgba32F vacated = dst.getPixel(0, 0);
  CHECK_APPROX(vacated, kTransparent, 1e-5f);
}

TEST(resample_90deg_rotation_round_trip_restores_source) {
  // Rotate by +90°, then rotate the result by -90°. The double round-trip
  // should approximately recover the original pixels.
  TuxImage src(4, 4);
  src.setPixel(0, 0, kRed);
  src.setPixel(3, 3, kGreen);
  src.setPixel(1, 2, Rgba32F{0.2f, 0.4f, 0.6f, 1.f});

  TuxImage mid(4, 4);
  resampleBilinear(src, mid, Affine2D::rotationAround(-kPi / 2.f, 2.f, 2.f));

  TuxImage back(4, 4);
  resampleBilinear(mid, back, Affine2D::rotationAround(kPi / 2.f, 2.f, 2.f));

  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      const Rgba32F s = src.getPixel(x, y);
      const Rgba32F b = back.getPixel(x, y);
      CHECK_APPROX(b, s, 1e-4f);
    }
  }
}

int main() { return tuxels::testing::run(); }
