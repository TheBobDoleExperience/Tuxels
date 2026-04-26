#include <cmath>

#include "core/Document.h"
#include "core/Pixel.h"
#include "core/SelectionMask.h"
#include "core/TuxImage.h"
#include "history/CropCommand.h"
#include "layers/LayerMask.h"
#include "layers/PixelLayer.h"
#include "test_harness.h"
#include "tools/CropTool.h"

namespace tuxels {

namespace {

constexpr Rgba32F kBlack{0.f, 0.f, 0.f, 1.f};
constexpr Rgba32F kRed{1.f, 0.f, 0.f, 1.f};
constexpr Rgba32F kGreen{0.f, 1.f, 0.f, 1.f};
constexpr Rgba32F kBlue{0.f, 0.f, 1.f, 1.f};

}  // namespace

TEST(crop_tool_drag_produces_pending_rect) {
  Document doc(100, 80);
  doc.addBlankPixelLayer("L");
  CropTool t;
  t.press(doc, 10.f, 20.f, MouseButton::Left);
  t.move(doc, 60.f, 65.f);
  t.release(doc, 60.f, 65.f, MouseButton::Left);
  auto c = t.takeCommit();
  CHECK(c.has_value());
  CHECK_EQ(c->rect.x, 10);
  CHECK_EQ(c->rect.y, 20);
  CHECK_EQ(c->rect.w, 51);  // inclusive pixel range 10..60
  CHECK_EQ(c->rect.h, 46);  // inclusive pixel range 20..65
}

TEST(crop_tool_degenerate_drag_yields_no_commit) {
  Document doc(100, 80);
  doc.addBlankPixelLayer("L");
  CropTool t;
  t.press(doc, 40.f, 40.f, MouseButton::Left);
  t.release(doc, 40.f, 40.f, MouseButton::Left);
  auto c = t.takeCommit();
  CHECK(!c.has_value());
}

TEST(crop_tool_clips_rect_to_document_bounds) {
  Document doc(40, 40);
  doc.addBlankPixelLayer("L");
  CropTool t;
  // Drag from outside top-left to past bottom-right; expect clipping.
  t.press(doc, -10.f, -5.f, MouseButton::Left);
  t.move(doc, 100.f, 100.f);
  t.release(doc, 100.f, 100.f, MouseButton::Left);
  auto c = t.takeCommit();
  CHECK(c.has_value());
  CHECK_EQ(c->rect.x, 0);
  CHECK_EQ(c->rect.y, 0);
  CHECK_EQ(c->rect.w, 40);
  CHECK_EQ(c->rect.h, 40);
}

TEST(crop_tool_live_rect_updates_during_drag) {
  Document doc(64, 64);
  doc.addBlankPixelLayer("L");
  CropTool t;
  CHECK(!t.liveRect().has_value());
  t.press(doc, 10.f, 10.f, MouseButton::Left);
  CHECK(t.liveRect().has_value());
  t.move(doc, 30.f, 40.f);
  auto lr = t.liveRect();
  CHECK(lr.has_value());
  CHECK_EQ(lr->x, 10);
  CHECK_EQ(lr->y, 10);
  CHECK_EQ(lr->w, 21);
  CHECK_EQ(lr->h, 31);
  t.release(doc, 30.f, 40.f, MouseButton::Left);
  CHECK(!t.liveRect().has_value());
}

TEST(crop_command_resizes_document) {
  Document doc(40, 40);
  auto* layer = doc.addBlankPixelLayer("L");
  layer->image.fill(kRed);

  CropCommand cmd(&doc, Rect{10, 10, 20, 20});
  CHECK_EQ(doc.width(), 20);
  CHECK_EQ(doc.height(), 20);
  // Old (10, 10) lands at (0, 0) of the new doc.
  CHECK_NEAR(layer->image.getPixel(0, 0).r, 1.f, 1e-5f);
  CHECK_NEAR(layer->image.getPixel(19, 19).r, 1.f, 1e-5f);
  // Old (30, 30) is past the new right edge.
  CHECK(!layer->image.inBounds(20, 20));
}

TEST(crop_command_translates_layer_content) {
  Document doc(40, 40);
  auto* layer = doc.addBlankPixelLayer("L");
  // A small red square at (12, 15) with size 3x4.
  for (int y = 15; y < 19; ++y) {
    for (int x = 12; x < 15; ++x) {
      layer->image.setPixel(x, y, kRed);
    }
  }

  CropCommand cmd(&doc, Rect{10, 10, 20, 20});
  // The red square should now be at (2, 5) of the cropped layer.
  CHECK_NEAR(layer->image.getPixel(2, 5).r, 1.f, 1e-5f);
  CHECK_NEAR(layer->image.getPixel(4, 8).r, 1.f, 1e-5f);
  // Outside the old square should be transparent.
  CHECK_NEAR(layer->image.getPixel(0, 0).a, 0.f, 1e-5f);
  CHECK_NEAR(layer->image.getPixel(10, 10).a, 0.f, 1e-5f);
}

TEST(crop_command_undo_restores_dimensions_and_content) {
  Document doc(40, 40);
  auto* layer = doc.addBlankPixelLayer("L");
  layer->image.fill(kGreen);
  // Distinguish a known pixel that would survive undo.
  layer->image.setPixel(35, 35, kBlue);

  CropCommand cmd(&doc, Rect{10, 10, 10, 10});
  CHECK_EQ(doc.width(), 10);
  cmd.undo();
  CHECK_EQ(doc.width(), 40);
  CHECK_EQ(doc.height(), 40);
  // Layer content restored.
  CHECK_NEAR(layer->image.getPixel(0, 0).g, 1.f, 1e-5f);
  CHECK_NEAR(layer->image.getPixel(35, 35).b, 1.f, 1e-5f);
}

TEST(crop_command_redo_reapplies_crop) {
  Document doc(40, 40);
  auto* layer = doc.addBlankPixelLayer("L");
  layer->image.fill(kGreen);

  CropCommand cmd(&doc, Rect{5, 5, 20, 20});
  cmd.undo();
  CHECK_EQ(doc.width(), 40);
  cmd.redo();
  CHECK_EQ(doc.width(), 20);
  CHECK_EQ(doc.height(), 20);
  CHECK_NEAR(layer->image.getPixel(0, 0).g, 1.f, 1e-5f);
}

TEST(crop_command_handles_multi_layer_document) {
  Document doc(50, 50);
  auto* bg = doc.addBlankPixelLayer("BG");
  bg->image.fill(kRed);
  auto* fg = doc.addBlankPixelLayer("FG");
  for (int y = 20; y < 30; ++y)
    for (int x = 20; x < 30; ++x) fg->image.setPixel(x, y, kGreen);

  CropCommand cmd(&doc, Rect{15, 15, 20, 20});
  CHECK_EQ(doc.width(), 20);
  // bg was solid red, so every pixel of the crop sub-rect is red.
  CHECK_NEAR(bg->image.getPixel(0, 0).r, 1.f, 1e-5f);
  CHECK_NEAR(bg->image.getPixel(19, 19).r, 1.f, 1e-5f);
  // fg's green square had original corner at (20,20); in cropped coords that's (5,5).
  CHECK_NEAR(fg->image.getPixel(5, 5).g, 1.f, 1e-5f);
  CHECK_NEAR(fg->image.getPixel(14, 14).g, 1.f, 1e-5f);
  // Outside the original green square is transparent.
  CHECK_NEAR(fg->image.getPixel(0, 0).a, 0.f, 1e-5f);

  cmd.undo();
  CHECK_EQ(doc.width(), 50);
  CHECK_NEAR(fg->image.getPixel(25, 25).g, 1.f, 1e-5f);
}

TEST(crop_command_crops_layer_mask) {
  Document doc(40, 40);
  auto* layer = doc.addBlankPixelLayer("L");
  layer->image.fill(kRed);
  auto mask = std::make_unique<LayerMask>(40, 40);
  // Hide the right half: R=0 on x>=20.
  for (int y = 0; y < 40; ++y)
    for (int x = 20; x < 40; ++x)
      mask->image.setPixel(x, y, Rgba32F(0.f, 0.f, 0.f, 1.f));
  layer->mask = std::move(mask);

  CropCommand cmd(&doc, Rect{10, 10, 20, 20});
  CHECK(layer->mask != nullptr);
  CHECK_EQ(layer->mask->image.width(), 20);
  CHECK_EQ(layer->mask->image.height(), 20);
  // In the new cropped mask, old x=20 (boundary) maps to new x=10.
  // Before x=10 should be reveal (absent → 1.0 or explicit 0 depending on
  // original source). Old (15,15) was untouched = absent = reveal, so
  // new (5,5) reads 0 from getPixel (absent tile).
  CHECK_NEAR(layer->mask->image.getPixel(15, 10).r, 0.f, 1e-5f);  // old (25,20) was hidden
  CHECK_NEAR(layer->mask->image.getPixel(19, 19).r, 0.f, 1e-5f);  // old (29,29) was hidden

  cmd.undo();
  CHECK_EQ(layer->mask->image.width(), 40);
  CHECK_NEAR(layer->mask->image.getPixel(25, 20).r, 0.f, 1e-5f);  // still hidden
}

TEST(crop_command_crops_selection) {
  Document doc(40, 40);
  doc.addBlankPixelLayer("L");
  auto sel = std::make_unique<SelectionMask>(40, 40);
  sel->fillRect(Rect{15, 15, 10, 10}, 1.f);
  doc.setSelection(std::move(sel));

  CropCommand cmd(&doc, Rect{10, 10, 20, 20});
  CHECK(doc.selection() != nullptr);
  CHECK_EQ(doc.selection()->width(), 20);
  // Old selection at (15,15..24,24) → new (5,5..14,14).
  CHECK_NEAR(doc.selection()->sample(5, 5), 1.f, 1e-5f);
  CHECK_NEAR(doc.selection()->sample(14, 14), 1.f, 1e-5f);
  CHECK_NEAR(doc.selection()->sample(15, 15), 0.f, 1e-5f);
  CHECK_NEAR(doc.selection()->sample(4, 4), 0.f, 1e-5f);

  cmd.undo();
  CHECK(doc.selection() != nullptr);
  CHECK_EQ(doc.selection()->width(), 40);
  CHECK_NEAR(doc.selection()->sample(20, 20), 1.f, 1e-5f);
}

TEST(crop_command_discards_selection_outside_crop) {
  Document doc(40, 40);
  doc.addBlankPixelLayer("L");
  auto sel = std::make_unique<SelectionMask>(40, 40);
  sel->fillRect(Rect{30, 30, 8, 8}, 1.f);  // entirely in the bottom-right
  doc.setSelection(std::move(sel));

  // Crop to top-left 20×20 — selection doesn't intersect.
  CropCommand cmd(&doc, Rect{0, 0, 20, 20});
  CHECK(doc.selection() == nullptr);

  cmd.undo();
  CHECK(doc.selection() != nullptr);
  CHECK_NEAR(doc.selection()->sample(32, 32), 1.f, 1e-5f);
}

TEST(crop_command_id_keyed_snapshot_survives_reorder) {
  // M5-S0: CropCommand's snapshot is keyed by LayerId. Reordering layers
  // between capture and undo must not misroute pixels — each layer's
  // pre-crop image returns to the layer that still carries the same id,
  // regardless of where it now lives in the stack.
  Document doc(64, 64);
  auto* a = doc.addBlankPixelLayer("A");
  auto* b = doc.addBlankPixelLayer("B");
  // Mark A solid red, B solid green so a swap mid-undo is visually obvious.
  for (int y = 0; y < 64; ++y) {
    for (int x = 0; x < 64; ++x) {
      a->image.setPixel(x, y, Rgba32F{1.f, 0.f, 0.f, 1.f});
      b->image.setPixel(x, y, Rgba32F{0.f, 1.f, 0.f, 1.f});
    }
  }
  const LayerId aId = a->id;
  const LayerId bId = b->id;

  CropCommand cmd(&doc, Rect{0, 0, 32, 32});
  // Reorder: swap A and B between capture and undo.
  doc.tree().move(0, 1);
  cmd.undo();

  // After undo: doc back to 64x64, each layer holds its original colors.
  CHECK_EQ(doc.width(), 64);
  CHECK_EQ(doc.height(), 64);
  LayerBase* aAfter = doc.tree().findById(aId);
  LayerBase* bAfter = doc.tree().findById(bId);
  CHECK(aAfter != nullptr);
  CHECK(bAfter != nullptr);
  auto* aPx = dynamic_cast<PixelLayer*>(aAfter);
  auto* bPx = dynamic_cast<PixelLayer*>(bAfter);
  CHECK(aPx != nullptr && bPx != nullptr);
  CHECK_NEAR(aPx->image.getPixel(20, 20).r, 1.f, 1e-5f);
  CHECK_NEAR(aPx->image.getPixel(20, 20).g, 0.f, 1e-5f);
  CHECK_NEAR(bPx->image.getPixel(20, 20).g, 1.f, 1e-5f);
  CHECK_NEAR(bPx->image.getPixel(20, 20).r, 0.f, 1e-5f);
}

TEST(crop_command_round_trip_preserves_tile_content) {
  // Regression: after a crop then undo, getPixel reads should match the
  // original layer tile-for-tile, including pixels on tile boundaries.
  Document doc(300, 300);
  auto* layer = doc.addBlankPixelLayer("L");
  // Write a unique pattern based on (x, y) so any mismatch is detectable.
  for (int y = 0; y < 300; ++y) {
    for (int x = 0; x < 300; ++x) {
      const float r = static_cast<float>((x * 7 + y * 3) % 251) / 250.f;
      layer->image.setPixel(x, y, Rgba32F{r, 0.5f, 0.25f, 1.f});
    }
  }

  CropCommand cmd(&doc, Rect{50, 50, 100, 100});
  cmd.undo();
  CHECK_EQ(doc.width(), 300);
  // Spot-check random pixels (including tile boundaries at 256).
  for (auto [x, y] : std::initializer_list<std::pair<int, int>>{
           {0, 0}, {255, 255}, {256, 256}, {299, 299}, {100, 200}, {200, 100}}) {
    const float expected = static_cast<float>((x * 7 + y * 3) % 251) / 250.f;
    CHECK_NEAR(layer->image.getPixel(x, y).r, expected, 1e-5f);
  }
}

}  // namespace tuxels

int main() { return tuxels::testing::run(); }
