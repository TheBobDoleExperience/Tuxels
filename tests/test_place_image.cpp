#include <cmath>
#include <memory>

#include "compositor/compose.h"
#include "core/Document.h"
#include "core/Pixel.h"
#include "core/TuxImage.h"
#include "layers/LayerBase.h"
#include "layers/PixelLayer.h"
#include "test_harness.h"

using namespace tuxels;

namespace {

constexpr Rgba32F kRed{1.f, 0.f, 0.f, 1.f};
constexpr Rgba32F kGreen{0.f, 1.f, 0.f, 1.f};

// Mirrors the centering formula in `MainWindow::onFilePlace` so the data-
// model tests here exercise the same math a user would see through the UI.
struct Centered {
  int ox, oy;
};
Centered centeredOrigin(int docW, int docH, int imgW, int imgH) {
  return {(docW - imgW) / 2, (docH - imgH) / 2};
}

TuxImage solid(int w, int h, Rgba32F c) {
  TuxImage img(w, h);
  img.fill(c);
  return img;
}

}  // namespace

TEST(place_small_image_centers_in_doc) {
  Document doc(200, 200);
  doc.addBlankPixelLayer("BG");

  const auto [ox, oy] = centeredOrigin(200, 200, 40, 40);
  auto* placed = doc.addPixelLayer(solid(40, 40, kGreen), ox, oy, "Placed");
  CHECK_EQ(placed->originX, 80);
  CHECK_EQ(placed->originY, 80);
  CHECK_EQ(placed->image.width(), 40);
  CHECK_EQ(placed->image.height(), 40);

  TuxImage out(200, 200);
  compose(doc.tree(), out);
  // Content occupies doc [80..120) × [80..120).
  CHECK(approxEqual(out.getPixel(80, 80), kGreen));
  CHECK(approxEqual(out.getPixel(119, 119), kGreen));
  CHECK_NEAR(out.getPixel(79, 79).a, 0.f, 1e-5f);
  CHECK_NEAR(out.getPixel(120, 120).a, 0.f, 1e-5f);
}

TEST(place_oversized_image_preserves_offscreen_pixels) {
  // Doc 100×100, placed image 180×180 — centered origin goes negative so
  // the image extends past every edge. The offscreen pixels must survive
  // in the layer's backing image so a future Move or doc-resize can reveal
  // them (that's the whole point of the S0 origin plumbing).
  Document doc(100, 100);
  auto srcImg = solid(180, 180, kRed);
  // Stamp a distinguishing pixel at (170, 170) — well past the doc bounds
  // when centered — so we can assert it round-trips.
  srcImg.setPixel(170, 170, kGreen);

  const auto [ox, oy] = centeredOrigin(100, 100, 180, 180);
  CHECK_EQ(ox, -40);
  CHECK_EQ(oy, -40);

  auto* placed = doc.addPixelLayer(std::move(srcImg), ox, oy, "Big");
  CHECK_EQ(placed->image.width(), 180);
  CHECK_EQ(placed->image.height(), 180);
  // Offscreen pixel still present in the layer's own coord space.
  CHECK(approxEqual(placed->image.getPixel(170, 170), kGreen));

  TuxImage out(100, 100);
  compose(doc.tree(), out);
  // Doc (0,0) → layer-local (40,40): red.
  CHECK(approxEqual(out.getPixel(0, 0), kRed));
  // Doc (99,99) → layer-local (139,139): red.
  CHECK(approxEqual(out.getPixel(99, 99), kRed));
  // The green distinguishing pixel at layer-local (170,170) maps to doc
  // (130,130), which is outside the 100×100 doc — it's preserved in the
  // layer but not visible in the composite.
  CHECK_NEAR(out.getPixel(99, 99).g, 0.f, 1e-5f);
}

TEST(place_then_undo_detaches_layer_and_restores_active_index) {
  // Simulates the LayerOpCommand closure in `MainWindow::onFilePlace` — a
  // shared stash holds the placed layer across undo (detach) and redo
  // (reattach). Verifies the tree state + active-index math that the
  // closure relies on.
  Document doc(100, 100);
  doc.addBlankPixelLayer("BG");
  const int prevActive = doc.activeLayerIndex();
  CHECK_EQ(prevActive, 0);

  // Apply (place).
  auto* placed = doc.addPixelLayer(solid(30, 30, kGreen), 10, 10, "Placed");
  CHECK_EQ(static_cast<int>(doc.tree().size()), 2);
  CHECK_EQ(doc.activeLayerIndex(), 1);
  (void)placed;

  // Undo: detach the last layer (what the closure's `undoIt` does).
  auto stash = doc.tree().removeAt(doc.tree().size() - 1);
  doc.setActiveLayerIndex(prevActive);
  CHECK_EQ(static_cast<int>(doc.tree().size()), 1);
  CHECK_EQ(doc.activeLayerIndex(), 0);
  CHECK(stash != nullptr);
  CHECK_EQ(stash->originX, 10);
  CHECK_EQ(stash->originY, 10);

  // Redo: reattach (what the closure's `doIt` does on the second call).
  doc.tree().add(std::move(stash));
  doc.setActiveLayerIndex(static_cast<int>(doc.tree().size()) - 1);
  CHECK_EQ(static_cast<int>(doc.tree().size()), 2);
  CHECK_EQ(doc.activeLayerIndex(), 1);
  auto* restored = dynamic_cast<PixelLayer*>(doc.tree().at(1));
  CHECK(restored != nullptr);
  CHECK_EQ(restored->originX, 10);
  CHECK_EQ(restored->originY, 10);
  CHECK(approxEqual(restored->image.getPixel(0, 0), kGreen));
}

TEST(place_image_smaller_than_one_pixel_centering_uses_floor_div) {
  // Doc 101×101, image 40×40. Integer division: (101-40)/2 = 30 (floors),
  // so centered origin is (30,30) — the extra pixel of slack lives on the
  // right/bottom. This matches user expectation and what Photoshop does.
  Document doc(101, 101);
  doc.addBlankPixelLayer("BG");
  const auto [ox, oy] = centeredOrigin(101, 101, 40, 40);
  CHECK_EQ(ox, 30);
  CHECK_EQ(oy, 30);
  auto* placed = doc.addPixelLayer(solid(40, 40, kRed), ox, oy, "Centered");
  CHECK_EQ(placed->originX, 30);
  CHECK_EQ(placed->originY, 30);
}

int main() { return tuxels::testing::run(); }
