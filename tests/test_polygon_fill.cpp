#include <cmath>
#include <vector>

#include "core/SelectionMask.h"
#include "test_harness.h"

using namespace tuxels;

namespace {

int countSelected(const SelectionMask& m, float threshold = 0.5f) {
  int n = 0;
  for (int y = 0; y < m.height(); ++y) {
    for (int x = 0; x < m.width(); ++x) {
      if (m.sample(x, y) > threshold) ++n;
    }
  }
  return n;
}

}  // namespace

TEST(fill_polygon_axis_aligned_rect_matches_fillRect) {
  // Integer-coord quad (10,10)-(40,10)-(40,30)-(10,30) should select the
  // 30×20 box between those corners. Matches fillRect behavior exactly.
  SelectionMask m(100, 100);
  std::vector<Point2f> quad = {
      {10.f, 10.f}, {40.f, 10.f}, {40.f, 30.f}, {10.f, 30.f}};
  m.fillPolygon(quad, 1.f);
  // Inside
  CHECK_NEAR(m.sample(10, 10), 1.f, 1e-5f);
  CHECK_NEAR(m.sample(39, 29), 1.f, 1e-5f);
  CHECK_NEAR(m.sample(25, 20), 1.f, 1e-5f);
  // Outside
  CHECK_NEAR(m.sample(9, 10), 0.f, 1e-5f);
  CHECK_NEAR(m.sample(40, 20), 0.f, 1e-5f);
  CHECK_NEAR(m.sample(10, 9), 0.f, 1e-5f);
  CHECK_NEAR(m.sample(10, 30), 0.f, 1e-5f);
  CHECK_EQ(countSelected(m), 30 * 20);
}

TEST(fill_polygon_degenerate_point_and_line_are_noops) {
  SelectionMask m(40, 40);
  std::vector<Point2f> p0;
  m.fillPolygon(p0, 1.f);
  CHECK_EQ(countSelected(m), 0);

  std::vector<Point2f> p1 = {{5.f, 5.f}};
  m.fillPolygon(p1, 1.f);
  CHECK_EQ(countSelected(m), 0);

  std::vector<Point2f> p2 = {{5.f, 5.f}, {20.f, 20.f}};
  m.fillPolygon(p2, 1.f);
  CHECK_EQ(countSelected(m), 0);
}

TEST(fill_polygon_triangle_pixel_count) {
  // Right triangle (0,0)-(10,0)-(0,10). Pixel centers (i+0.5, j+0.5) are
  // inside iff i + j <= 8 (strict below the hypotenuse y = 10 - x). That's
  // sum_{j=0..8} (9 - j) = 45 pixels — a Gauss sum of 9.
  SelectionMask m(20, 20);
  std::vector<Point2f> tri = {{0.f, 0.f}, {10.f, 0.f}, {0.f, 10.f}};
  m.fillPolygon(tri, 1.f);
  CHECK_EQ(countSelected(m), 45);
  // Spot-check: (0,0) is inside (center 0.5+0.5=1 < 10). (8,0) is the
  // last inside pixel at y=0 (center 8.5+0.5=9 < 10). (9,0) lands
  // *on* the hypotenuse (9.5+0.5=10) so the strict-interior rule
  // excludes it. (5,5) is well above the diagonal (5.5+5.5=11 > 10).
  CHECK_NEAR(m.sample(0, 0), 1.f, 1e-5f);
  CHECK_NEAR(m.sample(8, 0), 1.f, 1e-5f);
  CHECK_NEAR(m.sample(9, 0), 0.f, 1e-5f);
  CHECK_NEAR(m.sample(5, 5), 0.f, 1e-5f);
  CHECK_NEAR(m.sample(4, 4), 1.f, 1e-5f);
}

TEST(fill_polygon_concave_u_shape) {
  // U-shape: outer 20×20 box at (10,10)-(30,30) minus an inner tongue
  // from the top. Drawn as a single closed polygon via a CCW walk:
  //   (10,10) → (30,10) → (30,30) → (22,30) → (22,16)
  //          → (18,16) → (18,30) → (10,30) → close.
  // Expect: full 20×20 box minus the 4×14 interior notch at
  //   x in [18, 22), y in [16, 30) → 20*20 - 4*14 = 400 - 56 = 344.
  SelectionMask m(60, 60);
  std::vector<Point2f> u = {
      {10.f, 10.f}, {30.f, 10.f}, {30.f, 30.f}, {22.f, 30.f},
      {22.f, 16.f}, {18.f, 16.f}, {18.f, 30.f}, {10.f, 30.f}};
  m.fillPolygon(u, 1.f);
  CHECK_EQ(countSelected(m), 344);
  // The notch is unselected.
  CHECK_NEAR(m.sample(20, 20), 0.f, 1e-5f);
  // The bar above the notch is selected.
  CHECK_NEAR(m.sample(20, 12), 1.f, 1e-5f);
  // The side arms are selected.
  CHECK_NEAR(m.sample(12, 25), 1.f, 1e-5f);
  CHECK_NEAR(m.sample(28, 25), 1.f, 1e-5f);
}

TEST(fill_polygon_self_intersecting_even_odd_creates_hole) {
  // A bowtie-ish figure-eight where the crossing creates an even-odd
  // hole: outer loop encloses the full diamond, but the path passes
  // through itself so parity flips inside the central overlap.
  // Walk: (0,0)→(20,20)→(20,0)→(0,20)→ close. This creates two
  // triangles meeting at the center (10,10). Even-odd parity selects
  // both triangles, not the intersection.
  SelectionMask m(40, 40);
  std::vector<Point2f> bow = {
      {0.f, 0.f}, {20.f, 20.f}, {20.f, 0.f}, {0.f, 20.f}};
  m.fillPolygon(bow, 1.f);
  // Centers of the left and right triangles are selected.
  CHECK_NEAR(m.sample(3, 10), 1.f, 1e-5f);   // left triangle
  CHECK_NEAR(m.sample(17, 10), 1.f, 1e-5f);  // right triangle
  // Point near the crossing — inside the convex hull but outside both
  // triangle interiors (above both diagonals) is unselected.
  CHECK_NEAR(m.sample(10, 1), 0.f, 1e-5f);
  CHECK_NEAR(m.sample(10, 18), 0.f, 1e-5f);
}

TEST(fill_polygon_clamps_to_doc_bounds) {
  // Polygon extends past the doc right/bottom edge. Only the in-doc
  // portion is selected; no out-of-bounds writes.
  SelectionMask m(10, 10);
  std::vector<Point2f> big = {
      {-5.f, -5.f}, {25.f, -5.f}, {25.f, 25.f}, {-5.f, 25.f}};
  m.fillPolygon(big, 1.f);
  CHECK_EQ(countSelected(m), 10 * 10);
}

TEST(fill_polygon_crosses_tile_boundary) {
  // Polygon spans 3 tiles horizontally + 3 vertically to exercise the
  // tile-walker path. 256-px tiles → a box (100,100)-(700,700) touches
  // 3×3 tiles. 600×600 = 360000 pixels selected.
  SelectionMask m(800, 800);
  std::vector<Point2f> big = {
      {100.f, 100.f}, {700.f, 100.f}, {700.f, 700.f}, {100.f, 700.f}};
  m.fillPolygon(big, 1.f);
  CHECK_EQ(countSelected(m), 600 * 600);
  // Sanity: corner pixel inside; one-past-corner outside.
  CHECK_NEAR(m.sample(699, 699), 1.f, 1e-5f);
  CHECK_NEAR(m.sample(700, 700), 0.f, 1e-5f);
  CHECK_NEAR(m.sample(99, 99), 0.f, 1e-5f);
}

TEST(fill_polygon_partial_value) {
  SelectionMask m(20, 20);
  std::vector<Point2f> quad = {
      {0.f, 0.f}, {10.f, 0.f}, {10.f, 10.f}, {0.f, 10.f}};
  m.fillPolygon(quad, 0.5f);
  CHECK_NEAR(m.sample(5, 5), 0.5f, 1e-5f);
  CHECK_NEAR(m.sample(15, 15), 0.f, 1e-5f);
}

int main() { return tuxels::testing::run(); }
