#include <cmath>

#include "brush/BrushEngine.h"
#include "brush/RoundBrush.h"
#include "core/TuxImage.h"
#include "test_harness.h"

namespace tuxels {

TEST(round_brush_hard_edge_kernel) {
  RoundBrushParams p;
  p.diameter = 10;
  p.hardness = 1.f;
  RoundBrush b(p);
  // Near-center: kernel = 1.
  CHECK(b.kernel(5, 5) > 0.99f);
  CHECK(b.kernel(4, 5) > 0.99f);
  // Corner of the bounding box lies outside the disc radius.
  CHECK(b.kernel(0, 0) < 1e-6f);
  CHECK(b.kernel(9, 9) < 1e-6f);
}

TEST(round_brush_soft_falloff_kernel) {
  RoundBrushParams p;
  p.diameter = 32;
  p.hardness = 0.f;  // fully soft: smoothstep across radius
  RoundBrush b(p);
  CHECK(b.kernel(16, 16) > 0.95f);   // center bright
  CHECK(b.kernel(31, 16) < 0.05f);   // edge near-zero
  // Monotonic decrease moving outward from center along x axis.
  float prev = b.kernel(16, 16);
  for (int x = 17; x < 32; ++x) {
    const float cur = b.kernel(x, 16);
    CHECK(cur <= prev + 1e-5f);
    prev = cur;
  }
}

TEST(round_brush_spacing_px_rounds_to_at_least_one) {
  RoundBrushParams p;
  p.diameter = 20;
  p.spacingRatio = 0.1f;
  CHECK_EQ(RoundBrush(p).spacingPx(), 2);
  p.diameter = 3;
  p.spacingRatio = 0.1f;
  CHECK_EQ(RoundBrush(p).spacingPx(), 1);
  p.diameter = 100;
  p.spacingRatio = 0.25f;
  CHECK_EQ(RoundBrush(p).spacingPx(), 25);
}

TEST(brush_stamp_paints_center_pixel_at_full_alpha) {
  RoundBrushParams p;
  p.diameter = 5;
  p.hardness = 1.f;
  p.opacity = 1.f;
  p.flow = 1.f;
  p.color = Rgba32F(1.f, 0.f, 0.f, 1.f);
  RoundBrush brush(p);
  TuxImage img(10, 10);
  BrushEngine eng(brush, img);
  eng.beginStroke(5.f, 5.f);
  eng.endStroke();
  Rgba32F c = img.getPixel(5, 5);
  CHECK_NEAR(c.r, 1.f, 1e-4);
  CHECK_NEAR(c.a, 1.f, 1e-4);
}

TEST(brush_stroke_covers_midpoint_leaves_far_pixels_untouched) {
  RoundBrushParams p;
  p.diameter = 10;
  p.hardness = 1.f;
  p.opacity = 1.f;
  p.flow = 1.f;
  p.spacingRatio = 0.1f;  // 1-pixel spacing
  p.color = Rgba32F(1.f, 1.f, 1.f, 1.f);
  RoundBrush brush(p);
  TuxImage img(120, 20);
  BrushEngine eng(brush, img);
  eng.beginStroke(10.f, 10.f);
  eng.continueStroke(100.f, 10.f);
  eng.endStroke();

  Rect bb = eng.strokeBounds();
  CHECK(bb.w >= 90);

  Rgba32F mid = img.getPixel(55, 10);
  CHECK(mid.a > 0.95f);

  Rgba32F far_right = img.getPixel(115, 10);
  CHECK(far_right.a < 0.05f);
}

TEST(brush_stamp_handles_offscreen_clip) {
  RoundBrushParams p;
  p.diameter = 20;
  p.hardness = 1.f;
  p.opacity = 1.f;
  p.flow = 1.f;
  p.color = Rgba32F(0.f, 1.f, 0.f, 1.f);
  RoundBrush brush(p);
  TuxImage img(20, 20);
  BrushEngine eng(brush, img);
  // Center at (0,0) — three-quarters of the stamp is off-canvas.
  eng.beginStroke(0.f, 0.f);
  eng.endStroke();
  // The in-bounds corner pixel should still be painted.
  Rgba32F c = img.getPixel(0, 0);
  CHECK(c.a > 0.5f);
  // Pixel outside the stamp's reach is untouched.
  Rgba32F far = img.getPixel(19, 19);
  CHECK(far.a < 1e-4f);
}

TEST(brush_opacity_and_flow_scale_deposit) {
  RoundBrushParams p;
  p.diameter = 3;
  p.hardness = 1.f;
  p.opacity = 0.5f;
  p.flow = 0.5f;
  p.color = Rgba32F(1.f, 1.f, 1.f, 1.f);
  RoundBrush brush(p);
  TuxImage img(5, 5);
  BrushEngine eng(brush, img);
  eng.beginStroke(2.5f, 2.5f);
  eng.endStroke();
  // Expected alpha ≈ 1 * 0.5 * 0.5 * 1 = 0.25.
  Rgba32F c = img.getPixel(2, 2);
  CHECK_NEAR(c.a, 0.25f, 1e-4);
  CHECK_NEAR(c.r, 0.25f, 1e-4);
}

}  // namespace tuxels

int main() { return tuxels::testing::run(); }
