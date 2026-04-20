#include <cmath>
#include <utility>

#include "compositor/compose.h"
#include "core/Document.h"
#include "core/Pixel.h"
#include "core/TuxImage.h"
#include "layers/PixelLayer.h"
#include "test_harness.h"
#include "tools/ToolBase.h"
#include "tools/TransformTool.h"

using namespace tuxels;

namespace {

constexpr Rgba32F kRed{1.f, 0.f, 0.f, 1.f};
constexpr Rgba32F kGreen{0.f, 1.f, 0.f, 1.f};
constexpr float kPi = 3.14159265358979323846f;

PixelLayer* addSolidLayer(Document& doc, int w, int h, Rgba32F c, int ox,
                          int oy, const char* name) {
  TuxImage img(w, h);
  img.fill(c);
  return doc.addPixelLayer(std::move(img), ox, oy, name);
}

}  // namespace

TEST(transform_enter_captures_active_pixel_layer) {
  Document doc(200, 200);
  auto* layer = addSolidLayer(doc, 40, 60, kGreen, 30, 20, "L");
  TransformTool t;
  CHECK(t.enter(doc));
  CHECK(t.isActive());
  CHECK_NEAR(t.centerX(), 30.0 + 20.0, 1e-4);  // origin + srcW/2
  CHECK_NEAR(t.centerY(), 20.0 + 30.0, 1e-4);
  CHECK_NEAR(t.scaleX(), 1.0, 1e-5);
  CHECK_NEAR(t.scaleY(), 1.0, 1e-5);
  CHECK_NEAR(t.rotation(), 0.0, 1e-5);
  (void)layer;
}

TEST(transform_enter_fails_without_active_pixel_layer) {
  Document doc(64, 64);
  TransformTool t;
  CHECK(!t.enter(doc));
  CHECK(!t.isActive());
}

TEST(transform_identity_commit_is_no_op) {
  Document doc(100, 100);
  addSolidLayer(doc, 20, 20, kGreen, 10, 10, "L");
  TransformTool t;
  t.enter(doc);
  auto c = t.commit();
  CHECK(!c.has_value());
  CHECK(!t.isActive());
}

TEST(transform_translate_updates_origin_in_commit) {
  Document doc(200, 200);
  addSolidLayer(doc, 40, 40, kGreen, 50, 50, "L");
  TransformTool t;
  t.enter(doc);
  // Shift center by (+15, -10) — origin should move by the same delta.
  t.setTranslation(50.f + 20.f + 15.f, 50.f + 20.f - 10.f);
  auto c = t.commit();
  CHECK(c.has_value());
  CHECK_EQ(c->beforeX, 50);
  CHECK_EQ(c->beforeY, 50);
  CHECK_EQ(c->afterX, 65);
  CHECK_EQ(c->afterY, 40);
  CHECK_EQ(c->after.width(), 40);
  CHECK_EQ(c->after.height(), 40);
}

TEST(transform_uniform_scale_resizes_after_image) {
  Document doc(400, 400);
  addSolidLayer(doc, 40, 40, kGreen, 100, 100, "L");
  TransformTool t;
  t.enter(doc);
  t.setScale(2.f, 2.f);
  auto c = t.commit();
  CHECK(c.has_value());
  CHECK_EQ(c->after.width(), 80);
  CHECK_EQ(c->after.height(), 80);
  // Origin moves so the scaled bbox still straddles the same center
  // (120, 120): center − halfSize = 120 − 40 = 80.
  CHECK_EQ(c->afterX, 80);
  CHECK_EQ(c->afterY, 80);
  // Interior pixel should still be green (solid fill upscales cleanly).
  CHECK(approxEqual(c->after.getPixel(40, 40), kGreen, 1e-4f));
}

TEST(transform_90deg_rotation_swaps_dimensions) {
  Document doc(400, 400);
  addSolidLayer(doc, 20, 40, kGreen, 100, 100, "L");  // tall rectangle
  TransformTool t;
  t.enter(doc);
  t.setRotation(kPi / 2.f);
  auto c = t.commit();
  CHECK(c.has_value());
  // 90° rotation turns the 20×40 box into a 40×20 doc-space AABB.
  CHECK_EQ(c->after.width(), 40);
  CHECK_EQ(c->after.height(), 20);
  // Interior pixel should be green (solid fill).
  CHECK(approxEqual(c->after.getPixel(20, 10), kGreen, 1e-4f));
}

TEST(transform_cancel_discards_state_and_yields_no_commit) {
  Document doc(100, 100);
  auto* layer = addSolidLayer(doc, 20, 20, kGreen, 10, 10, "L");
  TransformTool t;
  t.enter(doc);
  t.setScale(3.f, 3.f);
  t.cancel();
  CHECK(!t.isActive());
  auto c = t.commit();
  CHECK(!c.has_value());
  // Real layer must be untouched.
  CHECK_EQ(layer->originX, 10);
  CHECK_EQ(layer->originY, 10);
  CHECK_EQ(layer->image.width(), 20);
}

TEST(transform_overlay_reports_active_override_and_corners) {
  Document doc(200, 200);
  addSolidLayer(doc, 40, 40, kGreen, 50, 50, "L");
  TransformTool t;
  t.enter(doc);
  auto o = t.overlay();
  CHECK(o.active);
  CHECK(o.layer.image != nullptr);
  // Identity transform → corners equal doc bbox corners of the source.
  CHECK_NEAR(o.corners[0][0], 50.0, 1e-4);
  CHECK_NEAR(o.corners[0][1], 50.0, 1e-4);
  CHECK_NEAR(o.corners[2][0], 90.0, 1e-4);
  CHECK_NEAR(o.corners[2][1], 90.0, 1e-4);
}

TEST(transform_press_on_corner_initiates_scale_drag) {
  // Press at a corner handle should put the tool in scale-drag mode;
  // dragging outward grows the transform.
  Document doc(400, 400);
  addSolidLayer(doc, 40, 40, kGreen, 100, 100, "L");
  TransformTool t;
  t.enter(doc);
  // TR corner of identity bbox is (140, 100). Press exactly there → scale.
  t.press(doc, 140.f, 100.f, MouseButton::Left);
  // Drag the TR handle to (160, 100) — +20 in x, center fixed → scaleX = 2.
  t.move(doc, 160.f, 100.f);
  CHECK_NEAR(t.scaleX(), 2.0, 1e-4);
  CHECK_NEAR(t.scaleY(), 1.0, 1e-4);
  t.release(doc, 160.f, 100.f, MouseButton::Left);
}

TEST(transform_press_inside_bbox_initiates_translate_drag) {
  Document doc(400, 400);
  addSolidLayer(doc, 40, 40, kGreen, 100, 100, "L");
  TransformTool t;
  t.enter(doc);
  // Press at doc (120, 120) — bbox interior. Drag to (130, 135).
  t.press(doc, 120.f, 120.f, MouseButton::Left);
  t.move(doc, 130.f, 135.f);
  // Center started at (120, 120); after drag delta (+10, +15) → (130, 135).
  CHECK_NEAR(t.centerX(), 130.0, 1e-4);
  CHECK_NEAR(t.centerY(), 135.0, 1e-4);
  t.release(doc, 130.f, 135.f, MouseButton::Left);
}

TEST(transform_press_outside_bbox_initiates_rotate_drag) {
  Document doc(400, 400);
  addSolidLayer(doc, 40, 40, kGreen, 100, 100, "L");
  TransformTool t;
  t.enter(doc);
  // Press far outside the bbox on the +x axis (center=(120,120)), drag to
  // +y axis; angle delta = +π/2.
  t.press(doc, 200.f, 120.f, MouseButton::Left);
  t.move(doc, 120.f, 200.f);
  CHECK_NEAR(t.rotation(), kPi / 2.0, 1e-3);
  t.release(doc, 120.f, 200.f, MouseButton::Left);
}

TEST(transform_commits_through_layer_override_preview) {
  // Sanity: the overlay's LayerOverride combined with compose() shows the
  // transformed content without mutating the real layer.
  Document doc(200, 200);
  auto* bg = addSolidLayer(doc, 200, 200, kRed, 0, 0, "BG");
  auto* layer = addSolidLayer(doc, 40, 40, kGreen, 80, 80, "L");
  (void)bg;
  TransformTool t;
  t.enter(doc);
  t.setTranslation(150.f, 150.f);  // move bbox center
  auto o = t.overlay();
  CHECK(o.active);

  TuxImage out(200, 200);
  compose(doc.tree(), out, &o.layer);
  // Green shows at the new position.
  CHECK(approxEqual(out.getPixel(150, 150), kGreen, 1e-4f));
  // Red shows at the original position.
  CHECK(approxEqual(out.getPixel(95, 95), kRed, 1e-4f));
  // Real layer is not mutated — its origin is still the original value.
  CHECK_EQ(layer->originX, 80);
  CHECK_EQ(layer->originY, 80);
}

int main() { return tuxels::testing::run(); }
