#include <memory>

#include "core/Document.h"
#include "core/Pixel.h"
#include "core/SelectionMask.h"
#include "core/TuxImage.h"
#include "layers/PixelLayer.h"
#include "test_harness.h"
#include "tools/SelectByColorTool.h"
#include "tools/ToolBase.h"

using namespace tuxels;

namespace {

constexpr Rgba32F kRed{1.f, 0.f, 0.f, 1.f};
constexpr Rgba32F kGreen{0.f, 1.f, 0.f, 1.f};
constexpr Rgba32F kAlmostRed{1.f, 0.1f, 0.f, 1.f};  // L∞ = 0.1 from kRed

// Build a 40×40 doc with one pixel layer that has a checkerboard of red and
// green cells at the caller's chosen origin. Useful for "select all red"
// style tests — reds and greens are not contiguous.
std::unique_ptr<Document> makeCheckerDoc(int docW, int docH, int layerW,
                                          int layerH, int ox, int oy) {
  auto doc = std::make_unique<Document>(docW, docH);
  TuxImage img(layerW, layerH);
  for (int y = 0; y < layerH; ++y) {
    for (int x = 0; x < layerW; ++x) {
      const bool evenCell = ((x / 4) + (y / 4)) % 2 == 0;
      img.setPixel(x, y, evenCell ? kRed : kGreen);
    }
  }
  doc->addPixelLayer(std::move(img), ox, oy, "L");
  return doc;
}

}  // namespace

TEST(select_by_color_tolerance_zero_matches_exact_color_only) {
  auto doc = makeCheckerDoc(40, 40, 40, 40, 0, 0);
  SelectByColorTool t;
  t.setTolerance(0.f);
  // Click on a red cell at doc-coord (2, 2).
  t.press(*doc, 2.f, 2.f, MouseButton::Left);
  auto c = t.takeCommit();
  CHECK(c.has_value());
  CHECK(c->after != nullptr);
  // Disjoint red cells — including ones not adjacent to the seed — should
  // all be selected.
  CHECK_NEAR(c->after->sample(2, 2), 1.f, 1e-5f);
  CHECK_NEAR(c->after->sample(10, 2), 1.f, 1e-5f);   // next red cell row 0
  CHECK_NEAR(c->after->sample(2, 10), 1.f, 1e-5f);   // next red cell row 2
  // Green cells stay unselected at tolerance 0.
  CHECK_NEAR(c->after->sample(6, 2), 0.f, 1e-5f);
  CHECK_NEAR(c->after->sample(2, 6), 0.f, 1e-5f);
}

TEST(select_by_color_full_tolerance_selects_all_opaque_pixels) {
  auto doc = makeCheckerDoc(40, 40, 40, 40, 0, 0);
  SelectByColorTool t;
  t.setTolerance(1.f);  // L∞ ≤ 1 catches every RGB in [0,1]
  t.press(*doc, 2.f, 2.f, MouseButton::Left);
  auto c = t.takeCommit();
  CHECK(c.has_value());
  CHECK(c->after != nullptr);
  // Both red and green cells now selected.
  CHECK_NEAR(c->after->sample(2, 2), 1.f, 1e-5f);
  CHECK_NEAR(c->after->sample(6, 2), 1.f, 1e-5f);
  CHECK_NEAR(c->after->sample(30, 30), 1.f, 1e-5f);
}

TEST(select_by_color_mid_tolerance_catches_near_colors) {
  auto doc = std::make_unique<Document>(40, 40);
  TuxImage img(40, 40);
  img.fill(kGreen);
  // Sparse red seed + a near-red at known coords.
  img.setPixel(5, 5, kRed);
  img.setPixel(20, 20, kAlmostRed);
  doc->addPixelLayer(std::move(img), 0, 0, "L");

  SelectByColorTool t;
  // Tolerance 0.05 — kAlmostRed (L∞ = 0.1 from red) should miss; raise
  // past 0.1 and it matches.
  t.setTolerance(0.05f);
  t.press(*doc, 5.f, 5.f, MouseButton::Left);
  {
    auto c = t.takeCommit();
    CHECK(c.has_value());
    CHECK(c->after != nullptr);
    CHECK_NEAR(c->after->sample(5, 5), 1.f, 1e-5f);
    CHECK_NEAR(c->after->sample(20, 20), 0.f, 1e-5f);  // too different
    CHECK_NEAR(c->after->sample(0, 0), 0.f, 1e-5f);    // green stays out
  }
  doc->setSelection(nullptr);
  t.setTolerance(0.15f);
  t.press(*doc, 5.f, 5.f, MouseButton::Left);
  {
    auto c = t.takeCommit();
    CHECK(c.has_value());
    CHECK_NEAR(c->after->sample(5, 5), 1.f, 1e-5f);
    CHECK_NEAR(c->after->sample(20, 20), 1.f, 1e-5f);  // now matches
  }
}

TEST(select_by_color_respects_layer_origin) {
  // Layer lives at doc origin (10, 20); its local (0,0) is doc (10, 20).
  auto doc = makeCheckerDoc(80, 80, 40, 40, 10, 20);
  SelectByColorTool t;
  t.setTolerance(0.f);
  // Doc (12, 22) → layer-local (2, 2) = red.
  t.press(*doc, 12.f, 22.f, MouseButton::Left);
  auto c = t.takeCommit();
  CHECK(c.has_value());
  CHECK(c->after != nullptr);
  // Selection at doc coordinates, offset by origin.
  CHECK_NEAR(c->after->sample(12, 22), 1.f, 1e-5f);
  CHECK_NEAR(c->after->sample(20, 22), 1.f, 1e-5f);   // another red cell
  // Outside the layer bbox → nothing written.
  CHECK_NEAR(c->after->sample(0, 0), 0.f, 1e-5f);
  CHECK_NEAR(c->after->sample(70, 70), 0.f, 1e-5f);
}

TEST(select_by_color_add_mode_unions_with_prior_selection) {
  auto doc = makeCheckerDoc(40, 40, 40, 40, 0, 0);
  auto prior = std::make_unique<SelectionMask>(40, 40);
  prior->fillRect(Rect{30, 0, 10, 10}, 1.f);
  doc->setSelection(std::move(prior));

  SelectByColorTool t;
  t.setMode(SelectionMode::Add);
  t.setTolerance(0.f);
  t.press(*doc, 2.f, 2.f, MouseButton::Left);  // red seed
  auto c = t.takeCommit();
  CHECK(c.has_value());
  CHECK(c->after != nullptr);
  // Prior region kept.
  CHECK_NEAR(c->after->sample(35, 5), 1.f, 1e-5f);
  // New reds added.
  CHECK_NEAR(c->after->sample(2, 2), 1.f, 1e-5f);
}

TEST(select_by_color_subtract_mode_clips_to_existing_selection) {
  auto doc = makeCheckerDoc(40, 40, 40, 40, 0, 0);
  auto prior = std::make_unique<SelectionMask>(40, 40);
  prior->fillRect(Rect{0, 0, 20, 20}, 1.f);
  doc->setSelection(std::move(prior));

  SelectByColorTool t;
  t.setMode(SelectionMode::Subtract);
  t.setTolerance(0.f);
  t.press(*doc, 2.f, 2.f, MouseButton::Left);  // red seed — seed is red
  auto c = t.takeCommit();
  CHECK(c.has_value());
  // Subtract with a clip: the pick is masked to the prior selection first,
  // then subtracted. So green pixels inside the prior stay selected, red
  // pixels inside the prior get removed, and pixels outside the prior are
  // unaffected.
  CHECK(c->after != nullptr);
  CHECK_NEAR(c->after->sample(2, 2), 0.f, 1e-5f);    // red inside prior → removed
  CHECK_NEAR(c->after->sample(6, 2), 1.f, 1e-5f);    // green inside prior → kept
  CHECK_NEAR(c->after->sample(25, 25), 0.f, 1e-5f);  // outside prior — unaffected
}

TEST(select_by_color_click_outside_layer_is_noop) {
  auto doc = makeCheckerDoc(80, 80, 40, 40, 10, 20);
  SelectByColorTool t;
  t.press(*doc, 5.f, 5.f, MouseButton::Left);  // outside layer bbox
  auto c = t.takeCommit();
  CHECK(!c.has_value());
}

TEST(select_by_color_click_without_pixel_layer_is_noop) {
  Document doc(40, 40);  // no active layer
  SelectByColorTool t;
  t.press(doc, 20.f, 20.f, MouseButton::Left);
  auto c = t.takeCommit();
  CHECK(!c.has_value());
}

int main() { return tuxels::testing::run(); }
