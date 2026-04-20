#include <cmath>

#include "brush/BrushEngine.h"
#include "brush/RoundBrush.h"
#include "core/SelectionMask.h"
#include "core/TuxImage.h"
#include "test_harness.h"

namespace tuxels {

TEST(selection_mask_sample_outside_doc_is_zero) {
  SelectionMask m(50, 50);
  m.fillRect(Rect{0, 0, 50, 50}, 1.f);
  CHECK_NEAR(m.sample(25, 25), 1.f, 1e-5f);
  CHECK_NEAR(m.sample(-1, 10), 0.f, 1e-5f);
  CHECK_NEAR(m.sample(10, -1), 0.f, 1e-5f);
  CHECK_NEAR(m.sample(50, 25), 0.f, 1e-5f);
  CHECK_NEAR(m.sample(25, 50), 0.f, 1e-5f);
}

TEST(selection_mask_fill_rect_inside_outside) {
  SelectionMask m(100, 100);
  m.fillRect(Rect{10, 20, 30, 40}, 1.f);
  // Inside
  CHECK_NEAR(m.sample(10, 20), 1.f, 1e-5f);
  CHECK_NEAR(m.sample(39, 59), 1.f, 1e-5f);
  CHECK_NEAR(m.sample(25, 40), 1.f, 1e-5f);
  // Outside (no tile or zeroed)
  CHECK_NEAR(m.sample(9, 20), 0.f, 1e-5f);
  CHECK_NEAR(m.sample(40, 20), 0.f, 1e-5f);
  CHECK_NEAR(m.sample(10, 19), 0.f, 1e-5f);
  CHECK_NEAR(m.sample(10, 60), 0.f, 1e-5f);
}

TEST(selection_mask_fill_clamps_to_doc_bounds) {
  SelectionMask m(50, 50);
  // Rect straddles right/bottom edge; only in-doc portion gets filled.
  m.fillRect(Rect{40, 40, 100, 100}, 1.f);
  CHECK_NEAR(m.sample(45, 45), 1.f, 1e-5f);
  CHECK_NEAR(m.sample(49, 49), 1.f, 1e-5f);
  CHECK_NEAR(m.sample(50, 50), 0.f, 1e-5f);  // outside doc
}

TEST(selection_mask_combine_add_max) {
  SelectionMask a(40, 40);
  a.fillRect(Rect{0, 0, 20, 40}, 1.f);
  SelectionMask b(40, 40);
  b.fillRect(Rect{10, 0, 20, 40}, 0.5f);
  a.combine(b, SelectionMode::Add);
  CHECK_NEAR(a.sample(5, 20), 1.f, 1e-5f);   // only in a
  CHECK_NEAR(a.sample(15, 20), 1.f, 1e-5f);  // both: max(1, 0.5) = 1
  CHECK_NEAR(a.sample(25, 20), 0.5f, 1e-5f); // only in b
  CHECK_NEAR(a.sample(35, 20), 0.f, 1e-5f);  // in neither
}

TEST(selection_mask_combine_subtract) {
  SelectionMask a(40, 40);
  a.fillRect(Rect{0, 0, 40, 40}, 1.f);
  SelectionMask b(40, 40);
  b.fillRect(Rect{10, 10, 20, 20}, 1.f);
  a.combine(b, SelectionMode::Subtract);
  CHECK_NEAR(a.sample(5, 5), 1.f, 1e-5f);   // unmodified
  CHECK_NEAR(a.sample(15, 15), 0.f, 1e-5f); // subtracted
  CHECK_NEAR(a.sample(30, 30), 1.f, 1e-5f); // just outside subtracted rect
}

TEST(selection_mask_combine_intersect) {
  SelectionMask a(40, 40);
  a.fillRect(Rect{0, 0, 30, 40}, 1.f);
  SelectionMask b(40, 40);
  b.fillRect(Rect{10, 0, 30, 40}, 1.f);
  a.combine(b, SelectionMode::Intersect);
  CHECK_NEAR(a.sample(5, 20), 0.f, 1e-5f);    // only in a
  CHECK_NEAR(a.sample(15, 20), 1.f, 1e-5f);   // in both
  CHECK_NEAR(a.sample(25, 20), 1.f, 1e-5f);   // in both
  CHECK_NEAR(a.sample(35, 20), 0.f, 1e-5f);   // only in b
}

TEST(selection_mask_combine_replace) {
  SelectionMask a(40, 40);
  a.fillRect(Rect{0, 0, 40, 40}, 1.f);
  SelectionMask b(40, 40);
  b.fillRect(Rect{10, 10, 20, 20}, 0.5f);
  a.combine(b, SelectionMode::Replace);
  CHECK_NEAR(a.sample(5, 5), 0.f, 1e-5f);    // dropped (b had no tile? same tile;
                                             // replace with 0 inside tile
                                             // where b is 0)
  CHECK_NEAR(a.sample(15, 15), 0.5f, 1e-5f); // overwritten
  CHECK_NEAR(a.sample(25, 25), 0.5f, 1e-5f); // overwritten
}

TEST(selection_mask_invert_round_trip) {
  SelectionMask m(30, 30);
  m.fillRect(Rect{0, 0, 15, 30}, 1.f);
  m.invert();
  CHECK_NEAR(m.sample(5, 10), 0.f, 1e-5f);
  CHECK_NEAR(m.sample(20, 10), 1.f, 1e-5f);
  m.invert();
  CHECK_NEAR(m.sample(5, 10), 1.f, 1e-5f);
  CHECK_NEAR(m.sample(20, 10), 0.f, 1e-5f);
}

TEST(selection_mask_clone_is_independent) {
  SelectionMask m(20, 20);
  m.fillRect(Rect{0, 0, 10, 20}, 1.f);
  auto c = m.clone();
  m.fillRect(Rect{10, 0, 10, 20}, 1.f);  // mutate original further
  CHECK_NEAR(c->sample(5, 10), 1.f, 1e-5f);
  CHECK_NEAR(c->sample(15, 10), 0.f, 1e-5f);  // clone unaffected
  CHECK_NEAR(m.sample(15, 10), 1.f, 1e-5f);
}

TEST(selection_mask_make_all_covers_doc) {
  auto m = SelectionMask::makeAll(100, 50);
  CHECK_NEAR(m->sample(0, 0), 1.f, 1e-5f);
  CHECK_NEAR(m->sample(99, 49), 1.f, 1e-5f);
  CHECK_NEAR(m->sample(50, 25), 1.f, 1e-5f);
}

TEST(brush_respects_empty_selection_null) {
  // null selection → paint normally everywhere.
  RoundBrushParams p;
  p.diameter = 5;
  p.hardness = 1.f;
  p.opacity = 1.f;
  p.flow = 1.f;
  p.color = Rgba32F{1.f, 0.f, 0.f, 1.f};
  RoundBrush brush(p);
  TuxImage img(20, 20);
  BrushEngine eng(brush, img, /*selection=*/nullptr);
  eng.applyStamp(10.f, 10.f);
  CHECK(img.getPixel(10, 10).r > 0.9f);
  CHECK(img.getPixel(10, 10).a > 0.9f);
}

TEST(brush_clips_outside_selection) {
  RoundBrushParams p;
  p.diameter = 6;
  p.hardness = 1.f;
  p.opacity = 1.f;
  p.flow = 1.f;
  p.color = Rgba32F{1.f, 0.f, 0.f, 1.f};
  RoundBrush brush(p);
  TuxImage img(20, 20);

  // Selection covers only the right half of the document.
  SelectionMask sel(20, 20);
  sel.fillRect(Rect{10, 0, 10, 20}, 1.f);

  BrushEngine eng(brush, img, &sel);
  // Stamp straddles the selection boundary at x=10.
  eng.applyStamp(10.f, 10.f);

  // Left-of-selection pixels untouched.
  CHECK_NEAR(img.getPixel(8, 10).a, 0.f, 1e-5f);
  CHECK_NEAR(img.getPixel(9, 10).a, 0.f, 1e-5f);
  // Right-of-selection pixels painted.
  CHECK(img.getPixel(11, 10).a > 0.5f);
  CHECK(img.getPixel(12, 10).a > 0.5f);
}

TEST(brush_scales_by_partial_selection) {
  RoundBrushParams p;
  p.diameter = 4;
  p.hardness = 1.f;
  p.opacity = 1.f;
  p.flow = 1.f;
  p.color = Rgba32F{1.f, 0.f, 0.f, 1.f};
  RoundBrush brush(p);

  TuxImage full(10, 10);
  BrushEngine engFull(brush, full, nullptr);
  engFull.applyStamp(5.f, 5.f);

  TuxImage half(10, 10);
  SelectionMask sel(10, 10);
  sel.fillRect(Rect{0, 0, 10, 10}, 0.5f);
  BrushEngine engHalf(brush, half, &sel);
  engHalf.applyStamp(5.f, 5.f);

  // Under 0.5 selection, a single stamp deposits half the alpha of the
  // un-clipped version (straight alpha over a transparent surface).
  const float af = full.getPixel(5, 5).a;
  const float ah = half.getPixel(5, 5).a;
  CHECK(af > 0.5f);
  CHECK_NEAR(ah, af * 0.5f, 1e-4f);
}

}  // namespace tuxels

int main() { return tuxels::testing::run(); }
