#include <cmath>
#include <utility>

#include "compositor/compose.h"
#include "core/Document.h"
#include "core/Pixel.h"
#include "core/TuxImage.h"
#include "history/TransformCommand.h"
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
  // Press inside the bbox but outside the pivot hit radius (bbox=100-140,
  // pivot at the center (120,120)). (110,110) is interior and clear of the
  // 8-px pivot dot and the 10-px corner hit regions.
  t.press(doc, 110.f, 110.f, MouseButton::Left);
  t.move(doc, 120.f, 125.f);
  // Center started at (120, 120); after drag delta (+10, +15) → (130, 135).
  CHECK_NEAR(t.centerX(), 130.0, 1e-4);
  CHECK_NEAR(t.centerY(), 135.0, 1e-4);
  t.release(doc, 120.f, 125.f, MouseButton::Left);
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

// Regression: the tool's live preview is an overlay, so the real layer is
// untouched through the drag. If the caller pushes the resulting
// TransformCommand without also running apply(), the layer silently
// reverts to its pre-transform image once the overlay clears. This test
// catches that by walking the full commit → TransformCommand → apply path
// and asserting the layer image matches the transformed scratch.
TEST(transform_shift_scale_locks_aspect_ratio) {
  // Shift-held scale drag forces |scaleY| = |scaleX|. The Y sign is
  // preserved so the user can still drag across the pivot to flip.
  Document doc(400, 400);
  addSolidLayer(doc, 40, 40, kGreen, 100, 100, "L");
  TransformTool t;
  t.enter(doc);
  // Grab TR corner at (140, 100), drag to (160, 80).
  // Without Shift: scaleX = 40/20 = 2, scaleY = -20/-20 = 1 (TR's local Y
  // is negative, cursor moved up further → positive sy).
  // With Shift: scaleY is locked to |scaleX| = 2, sign stays positive.
  t.press(doc, 140.f, 100.f, MouseButton::Left);
  t.setModifiers(Mod::Shift);
  t.move(doc, 160.f, 80.f);
  CHECK_NEAR(t.scaleX(), 2.0, 1e-4);
  CHECK_NEAR(t.scaleY(), 2.0, 1e-4);
  t.release(doc, 160.f, 80.f, MouseButton::Left);
}

TEST(transform_shift_scale_preserves_flip_sign) {
  // Dragging TR *past* the pivot flips Y — Shift should not clobber that
  // sign; it should only lock the magnitude.
  Document doc(400, 400);
  addSolidLayer(doc, 40, 40, kGreen, 100, 100, "L");
  TransformTool t;
  t.enter(doc);
  // Grab TR at (140, 100), drag to (160, 140) — dy = +40. TR's local Y is
  // -20 so sy = 40 / -20 = -2. Shift locks |sy| to |sx| = 2 but keeps sign.
  t.press(doc, 140.f, 100.f, MouseButton::Left);
  t.setModifiers(Mod::Shift);
  t.move(doc, 160.f, 140.f);
  CHECK_NEAR(t.scaleX(), 2.0, 1e-4);
  CHECK_NEAR(t.scaleY(), -2.0, 1e-4);
  t.release(doc, 160.f, 140.f, MouseButton::Left);
}

TEST(transform_shift_rotate_snaps_delta_to_15_degrees) {
  Document doc(400, 400);
  addSolidLayer(doc, 40, 40, kGreen, 100, 100, "L");
  TransformTool t;
  t.enter(doc);
  // Press on +x axis of the pivot (120, 120) — rotate drag.
  t.press(doc, 200.f, 120.f, MouseButton::Left);
  t.setModifiers(Mod::Shift);
  // Drag to ~16° around pivot — snap should pull it to exactly 15° (π/12).
  const float targetAngle = 16.f * kPi / 180.f;
  const float rx = 120.f + std::cos(targetAngle) * 80.f;
  const float ry = 120.f + std::sin(targetAngle) * 80.f;
  t.move(doc, rx, ry);
  const float snap = kPi / 12.f;
  CHECK_NEAR(t.rotation(), snap, 1e-4);
  t.release(doc, rx, ry, MouseButton::Left);
}

TEST(transform_pivot_drag_moves_pivot_only) {
  Document doc(400, 400);
  addSolidLayer(doc, 40, 40, kGreen, 100, 100, "L");
  TransformTool t;
  t.enter(doc);
  // Default pivot = bbox center = (120, 120). Press on the pivot dot, drag
  // to (140, 140). Only the pivot should move; scale/rotate/center stay.
  t.press(doc, 120.f, 120.f, MouseButton::Left);
  t.move(doc, 140.f, 140.f);
  CHECK_NEAR(t.pivotX(), 140.0, 1e-4);
  CHECK_NEAR(t.pivotY(), 140.0, 1e-4);
  CHECK_NEAR(t.scaleX(), 1.0, 1e-5);
  CHECK_NEAR(t.scaleY(), 1.0, 1e-5);
  CHECK_NEAR(t.rotation(), 0.0, 1e-5);
  CHECK_NEAR(t.centerX(), 120.0, 1e-4);
  CHECK_NEAR(t.centerY(), 120.0, 1e-4);
  t.release(doc, 140.f, 140.f, MouseButton::Left);
}

TEST(transform_rotate_after_pivot_drag_pivots_around_new_center) {
  Document doc(400, 400);
  addSolidLayer(doc, 40, 40, kGreen, 100, 100, "L");
  TransformTool t;
  t.enter(doc);
  // Move pivot from (120,120) to (140,140) — BR corner of the bbox.
  t.press(doc, 120.f, 120.f, MouseButton::Left);
  t.move(doc, 140.f, 140.f);
  t.release(doc, 140.f, 140.f, MouseButton::Left);
  // Rotate 90° about the new pivot. Press at (180, 120), drag to (160,
  // 180) — vector (40,-20) rotated by +π/2 about the pivot equals (20,40).
  t.press(doc, 180.f, 120.f, MouseButton::Left);
  t.move(doc, 160.f, 180.f);
  t.release(doc, 160.f, 180.f, MouseButton::Left);
  CHECK_NEAR(t.rotation(), kPi / 2.0, 1e-3);
  // TL corner at src=(0,0) should land at (180, 100) post-rotation about
  // (140, 140): local=(-40,-40), R(π/2)·(-40,-40) = (40,-40), +pivot
  // = (180, 100).
  auto o = t.overlay();
  CHECK(o.active);
  CHECK_NEAR(o.corners[0][0], 180.0, 1e-3);
  CHECK_NEAR(o.corners[0][1], 100.0, 1e-3);
  CHECK_NEAR(o.pivot[0], 140.0, 1e-4);
  CHECK_NEAR(o.pivot[1], 140.0, 1e-4);
}

TEST(transform_pending_commit_writes_through_apply) {
  Document doc(400, 400);
  auto* layer = addSolidLayer(doc, 40, 40, kGreen, 100, 100, "L");
  TransformTool t;
  t.enter(doc);
  t.setScale(2.f, 2.f);
  auto p = t.commit();
  CHECK(p.has_value());
  // Before apply, the layer is still the original 40×40 at (100,100).
  CHECK_EQ(layer->image.width(), 40);
  CHECK_EQ(layer->image.height(), 40);
  CHECK_EQ(layer->originX, 100);
  CHECK_EQ(layer->originY, 100);

  TransformCommand cmd(&doc, p->layerId, p->before, p->after, p->beforeX,
                       p->beforeY, p->afterX, p->afterY);
  cmd.apply();
  // After apply the layer must hold the scaled image + new origin.
  CHECK_EQ(layer->image.width(), 80);
  CHECK_EQ(layer->image.height(), 80);
  CHECK_EQ(layer->originX, 80);
  CHECK_EQ(layer->originY, 80);
  CHECK(approxEqual(layer->image.getPixel(40, 40), kGreen, 1e-4f));
}

int main() { return tuxels::testing::run(); }
