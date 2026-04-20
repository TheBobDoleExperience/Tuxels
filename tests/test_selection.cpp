#include <cmath>

#include "brush/BrushEngine.h"
#include "brush/RoundBrush.h"
#include "core/Document.h"
#include "core/SelectionMask.h"
#include "core/TuxImage.h"
#include "test_harness.h"
#include "tools/MarqueeTool.h"

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

TEST(selection_mask_is_empty_detects_zero) {
  SelectionMask m(40, 40);
  CHECK(m.isEmpty());
  m.fillRect(Rect{5, 5, 5, 5}, 1.f);
  CHECK(!m.isEmpty());
  m.fillRect(Rect{5, 5, 5, 5}, 0.f);
  CHECK(m.isEmpty());
}

TEST(selection_mask_bounds_of_selected) {
  SelectionMask m(100, 100);
  m.fillRect(Rect{10, 20, 5, 7}, 1.f);
  Rect b = m.boundsOfSelected();
  CHECK(b.x == 10);
  CHECK(b.y == 20);
  CHECK(b.w == 5);
  CHECK(b.h == 7);
  SelectionMask empty(100, 100);
  CHECK(empty.boundsOfSelected().isEmpty());
}

TEST(marquee_replace_creates_rect_selection) {
  Document doc(40, 40);
  MarqueeTool m;
  m.setModifiers(Mod::None);
  m.press(doc, 10.f, 15.f, MouseButton::Left);
  m.move(doc, 25.f, 30.f);
  m.release(doc, 25.f, 30.f, MouseButton::Left);
  auto c = m.takeCommit();
  CHECK(c.has_value());
  CHECK(!c->before);
  CHECK(c->after != nullptr);
  CHECK_NEAR(c->after->sample(10, 15), 1.f, 1e-5f);
  CHECK_NEAR(c->after->sample(25, 30), 1.f, 1e-5f);
  CHECK_NEAR(c->after->sample(9, 15), 0.f, 1e-5f);
  CHECK_NEAR(c->after->sample(26, 30), 0.f, 1e-5f);
}

TEST(marquee_add_union_with_existing) {
  Document doc(40, 40);
  auto initial = std::make_unique<SelectionMask>(40, 40);
  initial->fillRect(Rect{0, 0, 20, 20}, 1.f);
  doc.setSelection(std::move(initial));

  MarqueeTool m;
  m.setModifiers(Mod::Shift);
  m.press(doc, 15.f, 15.f, MouseButton::Left);
  m.move(doc, 30.f, 30.f);
  m.release(doc, 30.f, 30.f, MouseButton::Left);
  auto c = m.takeCommit();
  CHECK(c.has_value());
  CHECK(c->after != nullptr);
  // Union covers the old (0..20,0..20) and the new (15..30,15..30).
  CHECK_NEAR(c->after->sample(5, 5), 1.f, 1e-5f);    // old only
  CHECK_NEAR(c->after->sample(25, 25), 1.f, 1e-5f);  // new only
  CHECK_NEAR(c->after->sample(17, 17), 1.f, 1e-5f);  // overlap
  CHECK_NEAR(c->after->sample(35, 35), 0.f, 1e-5f);  // outside both
}

TEST(marquee_subtract_carves_hole) {
  Document doc(40, 40);
  auto initial = std::make_unique<SelectionMask>(40, 40);
  initial->fillRect(Rect{0, 0, 40, 40}, 1.f);
  doc.setSelection(std::move(initial));

  MarqueeTool m;
  m.setModifiers(Mod::Alt);
  m.press(doc, 10.f, 10.f, MouseButton::Left);
  m.move(doc, 19.f, 19.f);
  m.release(doc, 19.f, 19.f, MouseButton::Left);
  auto c = m.takeCommit();
  CHECK(c.has_value());
  CHECK(c->after != nullptr);
  CHECK_NEAR(c->after->sample(5, 5), 1.f, 1e-5f);    // still selected
  CHECK_NEAR(c->after->sample(15, 15), 0.f, 1e-5f);  // carved out
  CHECK_NEAR(c->after->sample(25, 25), 1.f, 1e-5f);  // still selected
}

TEST(marquee_intersect_keeps_overlap) {
  Document doc(40, 40);
  auto initial = std::make_unique<SelectionMask>(40, 40);
  initial->fillRect(Rect{0, 0, 20, 40}, 1.f);  // left half
  doc.setSelection(std::move(initial));

  MarqueeTool m;
  m.setModifiers(Mod::Shift | Mod::Alt);
  m.press(doc, 10.f, 0.f, MouseButton::Left);
  m.move(doc, 29.f, 39.f);  // right-ish rect crossing x=20
  m.release(doc, 29.f, 39.f, MouseButton::Left);
  auto c = m.takeCommit();
  CHECK(c.has_value());
  CHECK(c->after != nullptr);
  CHECK_NEAR(c->after->sample(5, 10), 0.f, 1e-5f);    // in a only
  CHECK_NEAR(c->after->sample(15, 10), 1.f, 1e-5f);   // in both
  CHECK_NEAR(c->after->sample(25, 10), 0.f, 1e-5f);   // in b only
}

TEST(marquee_subtract_all_collapses_to_null) {
  Document doc(20, 20);
  auto initial = std::make_unique<SelectionMask>(20, 20);
  initial->fillRect(Rect{0, 0, 20, 20}, 1.f);
  doc.setSelection(std::move(initial));

  MarqueeTool m;
  m.setModifiers(Mod::Alt);
  m.press(doc, 0.f, 0.f, MouseButton::Left);
  m.move(doc, 19.f, 19.f);
  m.release(doc, 19.f, 19.f, MouseButton::Left);
  auto c = m.takeCommit();
  CHECK(c.has_value());
  CHECK(c->before != nullptr);
  CHECK(c->after == nullptr);  // empty mask collapses to no-selection
}

TEST(marquee_click_without_drag_selects_single_pixel) {
  // A click-release without movement is still a valid 1-pixel rect drag.
  // The only way to produce a truly empty drag is to start+end outside the
  // document; that path is covered separately.
  Document doc(40, 40);
  MarqueeTool m;
  m.setModifiers(Mod::None);
  m.press(doc, 20.f, 20.f, MouseButton::Left);
  m.release(doc, 20.f, 20.f, MouseButton::Left);
  auto c = m.takeCommit();
  CHECK(c.has_value());
  CHECK(c->after != nullptr);
  CHECK_NEAR(c->after->sample(20, 20), 1.f, 1e-5f);
  CHECK_NEAR(c->after->sample(19, 20), 0.f, 1e-5f);
  CHECK_NEAR(c->after->sample(21, 20), 0.f, 1e-5f);
}

TEST(marquee_drag_outside_doc_replace_deselects) {
  Document doc(40, 40);
  auto initial = std::make_unique<SelectionMask>(40, 40);
  initial->fillRect(Rect{5, 5, 10, 10}, 1.f);
  doc.setSelection(std::move(initial));

  MarqueeTool m;
  m.setModifiers(Mod::None);
  // Drag entirely outside the document: clipped rect is empty, and a
  // Replace with an empty rect should collapse to "no selection".
  m.press(doc, -10.f, -10.f, MouseButton::Left);
  m.move(doc, -1.f, -1.f);
  m.release(doc, -1.f, -1.f, MouseButton::Left);
  auto c = m.takeCommit();
  CHECK(c.has_value());
  CHECK(c->before != nullptr);
  CHECK(c->after == nullptr);
}

TEST(marquee_persistent_mode_drives_combine_without_modifiers) {
  // When no modifier keys are held at press time, the marquee uses its
  // persistent mode (driven by the options-row buttons) instead of
  // Replace. This keeps Subtract/Intersect reachable on WMs that eat
  // Alt-drag for window-move.
  Document doc(40, 40);
  auto initial = std::make_unique<SelectionMask>(40, 40);
  initial->fillRect(Rect{0, 0, 20, 20}, 1.f);
  doc.setSelection(std::move(initial));

  MarqueeTool m;
  m.setMode(SelectionMode::Subtract);
  m.setModifiers(Mod::None);
  m.press(doc, 10.f, 10.f, MouseButton::Left);
  m.move(doc, 15.f, 15.f);
  m.release(doc, 15.f, 15.f, MouseButton::Left);
  auto c = m.takeCommit();
  CHECK(c.has_value());
  CHECK(c->after != nullptr);
  // Hole carved at (10..15, 10..15); (0,0) still selected, (10,10) not.
  CHECK_NEAR(c->after->sample(0, 0), 1.f, 1e-5f);
  CHECK_NEAR(c->after->sample(10, 10), 0.f, 1e-5f);
  CHECK_NEAR(c->after->sample(15, 15), 0.f, 1e-5f);
  CHECK_NEAR(c->after->sample(19, 19), 1.f, 1e-5f);
}

TEST(marquee_modifiers_override_persistent_mode) {
  // Modifiers held at press time win over the persistent mode — preserves
  // the Photoshop temporary-override semantics when the WM cooperates.
  Document doc(40, 40);
  auto initial = std::make_unique<SelectionMask>(40, 40);
  initial->fillRect(Rect{0, 0, 20, 20}, 1.f);
  doc.setSelection(std::move(initial));

  MarqueeTool m;
  m.setMode(SelectionMode::Subtract);  // persistent says subtract...
  m.setModifiers(Mod::Shift);          // ...but shift-press says add.
  m.press(doc, 25.f, 25.f, MouseButton::Left);
  m.move(doc, 30.f, 30.f);
  m.release(doc, 30.f, 30.f, MouseButton::Left);
  auto c = m.takeCommit();
  CHECK(c.has_value());
  CHECK(c->after != nullptr);
  // Both regions should be selected (union), proving Shift won over Subtract.
  CHECK_NEAR(c->after->sample(0, 0), 1.f, 1e-5f);
  CHECK_NEAR(c->after->sample(25, 25), 1.f, 1e-5f);
  CHECK_NEAR(c->after->sample(30, 30), 1.f, 1e-5f);
}

}  // namespace tuxels

int main() { return tuxels::testing::run(); }
