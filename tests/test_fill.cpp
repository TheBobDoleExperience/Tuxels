#include <cmath>

#include "core/Document.h"
#include "core/Pixel.h"
#include "core/SelectionMask.h"
#include "core/TuxImage.h"
#include "fill/FloodFill.h"
#include "layers/PixelLayer.h"
#include "test_harness.h"
#include "tools/BucketTool.h"
#include "tools/MagicWandTool.h"

namespace tuxels {

namespace {

constexpr Rgba32F kBlack{0.f, 0.f, 0.f, 1.f};
constexpr Rgba32F kRed{1.f, 0.f, 0.f, 1.f};
constexpr Rgba32F kGreen{0.f, 1.f, 0.f, 1.f};

}  // namespace

TEST(flood_fill_solid_region_no_barrier) {
  TuxImage img(32, 32);
  img.fill(kBlack);
  FloodFillOptions opts;
  opts.tolerance = 0.f;
  opts.opacity = 1.f;
  FloodFillResult r = floodFill(img, 15, 15, kRed, opts, nullptr);
  CHECK(r.changed);
  CHECK_EQ(r.pixelsFilled, 32 * 32);
  CHECK_EQ(r.bounds.x, 0);
  CHECK_EQ(r.bounds.y, 0);
  CHECK_EQ(r.bounds.w, 32);
  CHECK_EQ(r.bounds.h, 32);
  // Corners should be red now.
  CHECK_NEAR(img.getPixel(0, 0).r, 1.f, 1e-5f);
  CHECK_NEAR(img.getPixel(31, 31).r, 1.f, 1e-5f);
}

TEST(flood_fill_stops_at_color_barrier_zero_tolerance) {
  TuxImage img(32, 32);
  img.fill(kBlack);
  // Horizontal green line at y=16 — acts as a 1-pixel barrier with
  // tolerance=0 because green differs from black by 1.0.
  for (int x = 0; x < 32; ++x) img.setPixel(x, 16, kGreen);

  FloodFillOptions opts;
  FloodFillResult r = floodFill(img, 5, 5, kRed, opts, nullptr);
  CHECK(r.changed);
  // Top half should be red, bottom half and the barrier itself untouched.
  CHECK_NEAR(img.getPixel(0, 0).r, 1.f, 1e-5f);
  CHECK_NEAR(img.getPixel(31, 15).r, 1.f, 1e-5f);
  CHECK_NEAR(img.getPixel(10, 16).g, 1.f, 1e-5f);  // barrier still green
  CHECK_NEAR(img.getPixel(10, 16).r, 0.f, 1e-5f);
  CHECK_NEAR(img.getPixel(10, 17).r, 0.f, 1e-5f);  // below barrier unfilled
  CHECK_EQ(r.bounds.y, 0);
  CHECK_EQ(r.bounds.h, 16);  // only rows 0..15 filled
}

TEST(flood_fill_tolerance_crosses_near_matches) {
  TuxImage img(16, 16);
  // Gradient row: near-match should be within tolerance, clear mismatch not.
  img.fill(Rgba32F{0.1f, 0.1f, 0.1f, 1.f});
  img.setPixel(8, 8, Rgba32F{0.15f, 0.1f, 0.1f, 1.f});  // tiny delta

  FloodFillOptions opts;
  opts.tolerance = 0.1f;  // allows up to 0.1 per-channel
  FloodFillResult r = floodFill(img, 0, 0, kRed, opts, nullptr);
  CHECK(r.changed);
  CHECK_NEAR(img.getPixel(8, 8).r, 1.f, 1e-5f);  // crossed the near-match
}

TEST(flood_fill_respects_selection_as_hard_boundary) {
  TuxImage img(32, 32);
  img.fill(kBlack);
  auto sel = std::make_unique<SelectionMask>(32, 32);
  sel->fillRect(Rect{8, 8, 16, 16}, 1.f);

  FloodFillOptions opts;
  FloodFillResult r = floodFill(img, 15, 15, kRed, opts, sel.get());
  CHECK(r.changed);
  CHECK_EQ(r.pixelsFilled, 16 * 16);
  CHECK_NEAR(img.getPixel(15, 15).r, 1.f, 1e-5f);   // inside
  CHECK_NEAR(img.getPixel(7, 15).r, 0.f, 1e-5f);    // outside selection
  CHECK_NEAR(img.getPixel(15, 7).r, 0.f, 1e-5f);
}

TEST(flood_fill_seed_outside_selection_is_noop) {
  TuxImage img(16, 16);
  img.fill(kBlack);
  auto sel = std::make_unique<SelectionMask>(16, 16);
  sel->fillRect(Rect{0, 0, 4, 4}, 1.f);

  FloodFillOptions opts;
  FloodFillResult r = floodFill(img, 10, 10, kRed, opts, sel.get());
  CHECK(!r.changed);
  CHECK_EQ(r.pixelsFilled, 0);
}

TEST(flood_fill_opacity_blends_with_dst) {
  TuxImage img(4, 4);
  img.fill(kBlack);  // dst r=0
  FloodFillOptions opts;
  opts.opacity = 0.5f;
  FloodFillResult r = floodFill(img, 0, 0, kRed, opts, nullptr);
  CHECK(r.changed);
  // src_over with a=0.5: out.r = 0.5 * 1 + 0.5 * 0 = 0.5.
  CHECK_NEAR(img.getPixel(0, 0).r, 0.5f, 1e-5f);
}

TEST(flood_fill_bounds_are_tight_for_enclosed_island) {
  TuxImage img(64, 64);
  img.fill(kBlack);
  // Enclosed rectangle outline in green — interior is x=31..39, y=11..19.
  for (int x = 30; x <= 40; ++x) img.setPixel(x, 10, kGreen);  // top
  for (int x = 30; x <= 40; ++x) img.setPixel(x, 20, kGreen);  // bottom
  for (int y = 10; y <= 20; ++y) img.setPixel(30, y, kGreen);  // left
  for (int y = 10; y <= 20; ++y) img.setPixel(40, y, kGreen);  // right

  FloodFillOptions opts;
  FloodFillResult r = floodFill(img, 35, 15, kRed, opts, nullptr);
  CHECK(r.changed);
  CHECK_EQ(r.bounds.x, 31);
  CHECK_EQ(r.bounds.y, 11);
  CHECK_EQ(r.bounds.w, 9);   // x = 31..39
  CHECK_EQ(r.bounds.h, 9);   // y = 11..19
  CHECK_EQ(r.pixelsFilled, 9 * 9);
}

TEST(bucket_tool_fills_active_layer_with_fg_color) {
  Document doc(16, 16);
  auto* layer = doc.addBlankPixelLayer("L");
  layer->image.fill(kBlack);

  BucketTool bucket;
  bucket.setColor(kRed);
  bucket.setTolerance(0.f);
  bucket.setOpacity(1.f);
  bucket.press(doc, 8.f, 8.f, MouseButton::Left);
  bucket.release(doc, 8.f, 8.f, MouseButton::Left);

  auto fill = bucket.takeLastFill();
  CHECK(fill.layer == layer);
  CHECK(fill.target == &layer->image);
  CHECK_EQ(fill.bounds.w, 16);
  CHECK_EQ(fill.bounds.h, 16);
  CHECK_NEAR(layer->image.getPixel(0, 0).r, 1.f, 1e-5f);
  CHECK_NEAR(layer->image.getPixel(15, 15).r, 1.f, 1e-5f);
  // Recording must have captured at least one tile so undo has something.
  CHECK(!fill.recorded.before.empty());
}

TEST(bucket_tool_outside_selection_records_nothing) {
  Document doc(16, 16);
  auto* layer = doc.addBlankPixelLayer("L");
  layer->image.fill(kBlack);
  auto sel = std::make_unique<SelectionMask>(16, 16);
  sel->fillRect(Rect{0, 0, 4, 4}, 1.f);
  doc.setSelection(std::move(sel));

  BucketTool bucket;
  bucket.setColor(kRed);
  bucket.press(doc, 10.f, 10.f, MouseButton::Left);  // outside selection

  auto fill = bucket.takeLastFill();
  CHECK(fill.layer == nullptr);
  CHECK(fill.target == nullptr);
  // Pixel is still black (no fill happened).
  CHECK_NEAR(layer->image.getPixel(10, 10).r, 0.f, 1e-5f);
  // But the selected region wasn't touched either (we seeded outside it).
  CHECK_NEAR(layer->image.getPixel(2, 2).r, 0.f, 1e-5f);
}

TEST(bucket_tool_ignores_non_left_button) {
  Document doc(8, 8);
  doc.addBlankPixelLayer("L");
  BucketTool bucket;
  bucket.setColor(kRed);
  bucket.press(doc, 4.f, 4.f, MouseButton::Right);
  auto fill = bucket.takeLastFill();
  CHECK(fill.layer == nullptr);
}

TEST(flood_select_produces_mask_of_connected_region) {
  TuxImage img(32, 32);
  img.fill(kBlack);
  for (int x = 0; x < 32; ++x) img.setPixel(x, 16, kGreen);  // barrier

  auto mask = floodSelect(img, 5, 5, /*tolerance=*/0.f, nullptr);
  CHECK(mask != nullptr);
  // Top half selected, barrier + bottom half not.
  CHECK_NEAR(mask->sample(0, 0), 1.f, 1e-5f);
  CHECK_NEAR(mask->sample(31, 15), 1.f, 1e-5f);
  CHECK_NEAR(mask->sample(15, 16), 0.f, 1e-5f);
  CHECK_NEAR(mask->sample(15, 17), 0.f, 1e-5f);
}

TEST(flood_select_returns_null_when_nothing_matches) {
  TuxImage img(8, 8);
  img.fill(kBlack);
  auto mask = floodSelect(img, -1, 0, 0.f, nullptr);
  CHECK(mask == nullptr);
}

TEST(flood_select_respects_clip_selection) {
  TuxImage img(16, 16);
  img.fill(kBlack);
  auto clip = std::make_unique<SelectionMask>(16, 16);
  clip->fillRect(Rect{4, 4, 8, 8}, 1.f);
  auto mask = floodSelect(img, 8, 8, /*tolerance=*/0.f, clip.get());
  CHECK(mask != nullptr);
  CHECK_NEAR(mask->sample(8, 8), 1.f, 1e-5f);
  CHECK_NEAR(mask->sample(5, 5), 1.f, 1e-5f);
  CHECK_NEAR(mask->sample(3, 3), 0.f, 1e-5f);
  CHECK_NEAR(mask->sample(12, 12), 0.f, 1e-5f);
}

TEST(magic_wand_replace_selects_connected_region) {
  Document doc(32, 32);
  auto* layer = doc.addBlankPixelLayer("L");
  layer->image.fill(kBlack);
  for (int x = 0; x < 32; ++x) layer->image.setPixel(x, 16, kGreen);

  MagicWandTool w;
  w.setTolerance(0.f);
  w.setMode(SelectionMode::Replace);
  w.setModifiers(Mod::None);
  w.press(doc, 5.f, 5.f, MouseButton::Left);

  auto c = w.takeCommit();
  CHECK(c.has_value());
  CHECK(c->before == nullptr);
  CHECK(c->after != nullptr);
  CHECK_NEAR(c->after->sample(5, 5), 1.f, 1e-5f);
  CHECK_NEAR(c->after->sample(20, 20), 0.f, 1e-5f);
}

TEST(magic_wand_shift_adds_to_existing_selection) {
  Document doc(32, 32);
  auto* layer = doc.addBlankPixelLayer("L");
  layer->image.fill(kBlack);
  for (int x = 0; x < 32; ++x) layer->image.setPixel(x, 16, kGreen);

  auto initial = std::make_unique<SelectionMask>(32, 32);
  initial->fillRect(Rect{20, 20, 8, 8}, 1.f);
  doc.setSelection(std::move(initial));

  MagicWandTool w;
  w.setTolerance(0.f);
  w.setModifiers(Mod::Shift);  // shift-click → Add
  w.press(doc, 5.f, 5.f, MouseButton::Left);

  auto c = w.takeCommit();
  CHECK(c.has_value());
  CHECK(c->after != nullptr);
  // Original rect still selected.
  CHECK_NEAR(c->after->sample(23, 23), 1.f, 1e-5f);
  // Newly-wanded top half selected too.
  CHECK_NEAR(c->after->sample(5, 5), 1.f, 1e-5f);
  // Bottom half still unselected.
  CHECK_NEAR(c->after->sample(5, 25), 0.f, 1e-5f);
}

TEST(magic_wand_persistent_mode_drives_subtract) {
  Document doc(32, 32);
  auto* layer = doc.addBlankPixelLayer("L");
  layer->image.fill(kBlack);

  auto initial = std::make_unique<SelectionMask>(32, 32);
  initial->fillRect(Rect{0, 0, 32, 32}, 1.f);  // all selected
  doc.setSelection(std::move(initial));

  MagicWandTool w;
  w.setTolerance(0.f);
  w.setMode(SelectionMode::Subtract);
  w.setModifiers(Mod::None);
  w.press(doc, 16.f, 16.f, MouseButton::Left);

  auto c = w.takeCommit();
  CHECK(c.has_value());
  CHECK(c->after == nullptr);  // subtracting everything collapses to null
}

TEST(magic_wand_replace_outside_layer_noop) {
  Document doc(8, 8);
  doc.addBlankPixelLayer("L");
  MagicWandTool w;
  w.press(doc, -1.f, -1.f, MouseButton::Left);
  auto c = w.takeCommit();
  CHECK(!c.has_value());
}

TEST(bucket_fill_transparent_over_alpha_zero_makes_coverage) {
  // A fresh PixelLayer is born fully transparent; filling with opaque red
  // should produce (1, 0, 0, 1) — covering the empty canvas completely.
  Document doc(8, 8);
  auto* layer = doc.addBlankPixelLayer("L");
  // Ensure starting alpha is 0 everywhere (default pixel is transparent).
  CHECK_NEAR(layer->image.getPixel(0, 0).a, 0.f, 1e-5f);

  BucketTool bucket;
  bucket.setColor(kRed);
  bucket.press(doc, 4.f, 4.f, MouseButton::Left);
  auto fill = bucket.takeLastFill();
  CHECK(fill.layer == layer);
  CHECK_NEAR(layer->image.getPixel(0, 0).r, 1.f, 1e-5f);
  CHECK_NEAR(layer->image.getPixel(0, 0).a, 1.f, 1e-5f);
}

}  // namespace tuxels

int main() { return tuxels::testing::run(); }
