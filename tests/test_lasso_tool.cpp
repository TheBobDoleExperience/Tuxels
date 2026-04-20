#include <memory>

#include "core/Document.h"
#include "core/SelectionMask.h"
#include "test_harness.h"
#include "tools/LassoTool.h"
#include "tools/PolyLassoTool.h"
#include "tools/ToolBase.h"

using namespace tuxels;

namespace {

Document makeDoc(int w = 64, int h = 64) {
  Document d(w, h);
  d.addBlankPixelLayer("L");
  return d;
}

std::unique_ptr<SelectionMask> solidRectMask(int docW, int docH, Rect r) {
  auto m = std::make_unique<SelectionMask>(docW, docH);
  m->fillRect(r, 1.f);
  return m;
}

}  // namespace

// ------------------------- LassoTool -------------------------

TEST(lasso_rectangular_drag_commits_polygon) {
  Document doc = makeDoc(64, 64);
  LassoTool t;
  // Four-corner rectangle: (10,10)→(30,10)→(30,20)→(10,20).
  t.press(doc, 10.f, 10.f, MouseButton::Left);
  t.move(doc, 30.f, 10.f);
  t.move(doc, 30.f, 20.f);
  t.move(doc, 10.f, 20.f);
  t.release(doc, 10.f, 10.f, MouseButton::Left);
  auto c = t.takeCommit();
  CHECK(c.has_value());
  CHECK(c->after != nullptr);
  CHECK(c->before == nullptr);  // nothing was selected before
  // Interior pixel selected; exterior not.
  CHECK_NEAR(c->after->sample(15, 15), 1.f, 1e-5f);
  CHECK_NEAR(c->after->sample(5, 5), 0.f, 1e-5f);
  CHECK_NEAR(c->after->sample(35, 15), 0.f, 1e-5f);
}

TEST(lasso_empty_click_no_prior_selection_is_noop) {
  Document doc = makeDoc();
  LassoTool t;
  t.press(doc, 5.f, 5.f, MouseButton::Left);
  t.release(doc, 5.f, 5.f, MouseButton::Left);
  auto c = t.takeCommit();
  CHECK(!c.has_value());
}

TEST(lasso_empty_click_replace_mode_deselects_existing) {
  Document doc = makeDoc();
  doc.setSelection(solidRectMask(64, 64, Rect{0, 0, 10, 10}));
  LassoTool t;
  t.setMode(SelectionMode::Replace);
  t.press(doc, 20.f, 20.f, MouseButton::Left);
  t.release(doc, 20.f, 20.f, MouseButton::Left);
  auto c = t.takeCommit();
  CHECK(c.has_value());
  CHECK(c->before != nullptr);
  CHECK(c->after == nullptr);  // Deselect
}

TEST(lasso_add_mode_unions_with_prior_selection) {
  Document doc = makeDoc();
  doc.setSelection(solidRectMask(64, 64, Rect{0, 0, 10, 10}));
  LassoTool t;
  t.setMode(SelectionMode::Add);
  // Triangle far from the prior rect.
  t.press(doc, 30.f, 30.f, MouseButton::Left);
  t.move(doc, 50.f, 30.f);
  t.move(doc, 40.f, 50.f);
  t.release(doc, 30.f, 30.f, MouseButton::Left);
  auto c = t.takeCommit();
  CHECK(c.has_value());
  CHECK(c->after != nullptr);
  // Both regions present in the union.
  CHECK_NEAR(c->after->sample(5, 5), 1.f, 1e-5f);    // original rect
  CHECK_NEAR(c->after->sample(40, 40), 1.f, 1e-5f);  // new triangle interior
}

TEST(lasso_shift_modifier_at_press_forces_add) {
  Document doc = makeDoc();
  doc.setSelection(solidRectMask(64, 64, Rect{0, 0, 10, 10}));
  LassoTool t;
  t.setMode(SelectionMode::Replace);       // persistent wants Replace…
  t.setModifiers(Mod::Shift);               // …but Shift at press wins.
  t.press(doc, 30.f, 30.f, MouseButton::Left);
  t.setModifiers(Mod::None);
  t.move(doc, 50.f, 30.f);
  t.move(doc, 40.f, 50.f);
  t.release(doc, 30.f, 30.f, MouseButton::Left);
  auto c = t.takeCommit();
  CHECK(c.has_value());
  CHECK(c->after != nullptr);
  CHECK_NEAR(c->after->sample(5, 5), 1.f, 1e-5f);    // prior kept
  CHECK_NEAR(c->after->sample(40, 40), 1.f, 1e-5f);  // new added
}

TEST(lasso_live_path_visible_during_drag_hidden_after_release) {
  Document doc = makeDoc();
  LassoTool t;
  t.press(doc, 10.f, 10.f, MouseButton::Left);
  t.move(doc, 20.f, 10.f);
  t.move(doc, 20.f, 20.f);
  auto live = t.livePath();
  CHECK(live.has_value());
  CHECK(live->size() >= 3);
  t.release(doc, 10.f, 20.f, MouseButton::Left);
  CHECK(!t.livePath().has_value());
}

// ------------------------- PolyLassoTool -------------------------

TEST(poly_lasso_click_sequence_then_finish_commits) {
  Document doc = makeDoc();
  PolyLassoTool t;
  t.press(doc, 10.f, 10.f, MouseButton::Left);
  t.press(doc, 30.f, 10.f, MouseButton::Left);
  t.press(doc, 30.f, 30.f, MouseButton::Left);
  t.press(doc, 10.f, 30.f, MouseButton::Left);
  CHECK(t.isBuilding());
  t.finish(doc);
  CHECK(!t.isBuilding());
  auto c = t.takeCommit();
  CHECK(c.has_value());
  CHECK(c->after != nullptr);
  CHECK_NEAR(c->after->sample(20, 20), 1.f, 1e-5f);  // interior
  CHECK_NEAR(c->after->sample(5, 5), 0.f, 1e-5f);    // exterior
}

TEST(poly_lasso_click_near_start_closes_polygon) {
  Document doc = makeDoc();
  PolyLassoTool t;
  t.press(doc, 10.f, 10.f, MouseButton::Left);
  t.press(doc, 30.f, 10.f, MouseButton::Left);
  t.press(doc, 30.f, 30.f, MouseButton::Left);
  // Click within 6 doc px of the first vertex (10,10) — should auto-close.
  t.press(doc, 11.f, 11.f, MouseButton::Left);
  CHECK(!t.isBuilding());
  auto c = t.takeCommit();
  CHECK(c.has_value());
  CHECK(c->after != nullptr);
  // Triangle (10,10)→(30,10)→(30,30). Sample a pixel clearly in the
  // interior — (20,20)'s center (20.5, 20.5) lies exactly on the diagonal
  // edge, so use (25,12) which is unambiguously inside.
  CHECK_NEAR(c->after->sample(25, 12), 1.f, 1e-5f);
}

TEST(poly_lasso_cancel_discards_without_commit) {
  Document doc = makeDoc();
  PolyLassoTool t;
  t.press(doc, 10.f, 10.f, MouseButton::Left);
  t.press(doc, 30.f, 10.f, MouseButton::Left);
  t.press(doc, 30.f, 30.f, MouseButton::Left);
  t.cancel();
  CHECK(!t.isBuilding());
  auto c = t.takeCommit();
  CHECK(!c.has_value());
  CHECK(!t.livePath().has_value());
}

TEST(poly_lasso_hover_appends_cursor_to_live_path) {
  Document doc = makeDoc();
  PolyLassoTool t;
  t.press(doc, 10.f, 10.f, MouseButton::Left);
  t.press(doc, 30.f, 10.f, MouseButton::Left);
  t.hover(doc, 25.f, 25.f);
  auto live = t.livePath();
  CHECK(live.has_value());
  CHECK_EQ(live->size(), static_cast<size_t>(3));  // 2 vertices + cursor
  CHECK_NEAR(live->back().x, 25.f, 1e-5f);
  CHECK_NEAR(live->back().y, 25.f, 1e-5f);
}

TEST(poly_lasso_finish_with_two_vertices_is_noop) {
  Document doc = makeDoc();
  PolyLassoTool t;
  t.press(doc, 10.f, 10.f, MouseButton::Left);
  t.press(doc, 30.f, 10.f, MouseButton::Left);
  t.finish(doc);
  CHECK(!t.isBuilding());
  auto c = t.takeCommit();
  CHECK(!c.has_value());
}

TEST(poly_lasso_finish_two_vertices_with_prior_replace_deselects) {
  Document doc = makeDoc();
  doc.setSelection(solidRectMask(64, 64, Rect{0, 0, 10, 10}));
  PolyLassoTool t;
  t.setMode(SelectionMode::Replace);
  t.press(doc, 40.f, 40.f, MouseButton::Left);
  t.press(doc, 50.f, 40.f, MouseButton::Left);
  t.finish(doc);
  auto c = t.takeCommit();
  CHECK(c.has_value());
  CHECK(c->before != nullptr);
  CHECK(c->after == nullptr);
}

int main() { return tuxels::testing::run(); }
