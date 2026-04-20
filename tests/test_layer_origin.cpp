#include <cmath>
#include <memory>

#include "compositor/compose.h"
#include "core/Document.h"
#include "core/Pixel.h"
#include "core/TuxImage.h"
#include "history/CropCommand.h"
#include "layers/LayerMask.h"
#include "layers/LayerTree.h"
#include "layers/PixelLayer.h"
#include "test_harness.h"

using namespace tuxels;

namespace {

constexpr Rgba32F kRed{1.f, 0.f, 0.f, 1.f};
constexpr Rgba32F kGreen{0.f, 1.f, 0.f, 1.f};
constexpr Rgba32F kBlue{0.f, 0.f, 1.f, 1.f};

std::unique_ptr<PixelLayer> solid(int w, int h, Rgba32F c, LayerId id) {
  auto l = std::make_unique<PixelLayer>(w, h);
  l->id = id;
  l->image.fill(c);
  return l;
}

}  // namespace

TEST(origin_zero_matches_legacy_behavior) {
  // Regression floor: a layer at origin (0,0) with doc-sized image must
  // composite pixel-for-pixel identically to the M1 path.
  LayerTree tree;
  tree.add(solid(32, 32, kRed, 1));
  TuxImage out(32, 32);
  compose(tree, out);
  CHECK(approxEqual(out.getPixel(0, 0), kRed));
  CHECK(approxEqual(out.getPixel(31, 31), kRed));
}

TEST(layer_smaller_than_doc_leaves_outside_transparent) {
  LayerTree tree;
  auto l = solid(20, 20, kGreen, 1);
  l->originX = 5;
  l->originY = 5;
  tree.add(std::move(l));
  TuxImage out(40, 40);
  compose(tree, out);
  // Inside the layer rect [5..25) × [5..25): green.
  CHECK(approxEqual(out.getPixel(5, 5), kGreen));
  CHECK(approxEqual(out.getPixel(24, 24), kGreen));
  // Outside: transparent.
  CHECK_NEAR(out.getPixel(4, 4).a, 0.f, 1e-5f);
  CHECK_NEAR(out.getPixel(25, 25).a, 0.f, 1e-5f);
  CHECK_NEAR(out.getPixel(0, 30).a, 0.f, 1e-5f);
}

TEST(layer_extending_past_doc_clips_correctly) {
  // 40×40 layer at origin (-10,-10) in a 40×40 doc. Only [0..30) × [0..30)
  // of the doc should be covered.
  LayerTree tree;
  auto l = solid(40, 40, kBlue, 1);
  l->originX = -10;
  l->originY = -10;
  tree.add(std::move(l));
  TuxImage out(40, 40);
  compose(tree, out);
  CHECK(approxEqual(out.getPixel(0, 0), kBlue));
  CHECK(approxEqual(out.getPixel(29, 29), kBlue));
  CHECK_NEAR(out.getPixel(30, 30).a, 0.f, 1e-5f);
  CHECK_NEAR(out.getPixel(39, 39).a, 0.f, 1e-5f);
}

TEST(layer_fully_offscreen_contributes_nothing) {
  LayerTree tree;
  auto l = solid(20, 20, kRed, 1);
  l->originX = 100;  // past doc's 40×40 extent
  l->originY = 100;
  tree.add(std::move(l));
  TuxImage out(40, 40);
  compose(tree, out);
  CHECK_NEAR(out.getPixel(0, 0).a, 0.f, 1e-5f);
  CHECK_NEAR(out.getPixel(39, 39).a, 0.f, 1e-5f);
}

TEST(layer_crosses_tile_boundary_at_unaligned_origin) {
  // Doc is 512×512 (2×2 tiles). A 100×100 layer placed at (200,200) spans
  // all four tiles of the doc since it crosses the 256 boundary.
  LayerTree tree;
  auto l = solid(100, 100, kGreen, 1);
  l->originX = 200;
  l->originY = 200;
  tree.add(std::move(l));
  TuxImage out(512, 512);
  compose(tree, out);
  CHECK(approxEqual(out.getPixel(200, 200), kGreen));  // tile (0,0)
  CHECK(approxEqual(out.getPixel(255, 255), kGreen));  // still tile (0,0)
  CHECK(approxEqual(out.getPixel(256, 256), kGreen));  // tile (1,1)
  CHECK(approxEqual(out.getPixel(299, 299), kGreen));  // tile (1,1)
  CHECK_NEAR(out.getPixel(300, 300).a, 0.f, 1e-5f);     // just past layer
}

TEST(mask_with_layer_origin_is_applied_in_doc_space) {
  // Layer at origin (10,10), 30×30 green. Mask (same dims) hides x>=15 in
  // layer-local — which is x>=25 in doc coords.
  LayerTree tree;
  tree.add(solid(64, 64, kRed, 1));  // red background
  auto top = solid(30, 30, kGreen, 2);
  top->originX = 10;
  top->originY = 10;
  top->mask = std::make_unique<LayerMask>(30, 30);
  // Fill reveal first, then paint the right half to hide. Masks default
  // absent tiles to reveal, but any tile touched by setPixel materializes
  // with R=0 elsewhere — so an explicit reveal pass is required when a
  // single tile would otherwise hold both reveal and hide pixels.
  top->mask->image.fill(Rgba32F(1.f, 1.f, 1.f, 1.f));
  for (int y = 0; y < 30; ++y) {
    for (int x = 15; x < 30; ++x) {
      top->mask->image.setPixel(x, y, Rgba32F(0.f, 0.f, 0.f, 1.f));
    }
  }
  tree.add(std::move(top));
  TuxImage out(64, 64);
  compose(tree, out);
  // Doc (20,20) → layer-local (10,10) → mask reveal (absent tile = 1.0):
  // green wins over red.
  CHECK(approxEqual(out.getPixel(20, 20), kGreen));
  // Doc (30,20) → layer-local (20,10) → mask = 0: red shows through.
  CHECK(approxEqual(out.getPixel(30, 20), kRed));
  // Doc (5,5) is outside the green layer entirely: red.
  CHECK(approxEqual(out.getPixel(5, 5), kRed));
}

TEST(crop_translates_offset_layer_origin_into_new_doc) {
  Document doc(100, 100);
  // A 20×20 red layer placed at origin (40,40) — content rect [40..60).
  auto layer = solid(20, 20, kRed, 1);
  layer->originX = 40;
  layer->originY = 40;
  doc.tree().add(std::move(layer));
  doc.setActiveLayerIndex(0);

  // Crop to [30..80) × [30..80). Layer content [40..60) survives entirely.
  // New origin = (40-30, 40-30) = (10,10).
  CropCommand cmd(&doc, Rect{30, 30, 50, 50});
  CHECK_EQ(doc.width(), 50);
  CHECK_EQ(doc.height(), 50);
  auto* pxl = dynamic_cast<PixelLayer*>(doc.tree().at(0));
  CHECK(pxl != nullptr);
  CHECK_EQ(pxl->originX, 10);
  CHECK_EQ(pxl->originY, 10);
  CHECK_EQ(pxl->image.width(), 20);
  CHECK_EQ(pxl->image.height(), 20);

  // Composite: doc (10..30) × (10..30) should be red; elsewhere transparent.
  TuxImage out(50, 50);
  compose(doc.tree(), out);
  CHECK(approxEqual(out.getPixel(10, 10), kRed));
  CHECK(approxEqual(out.getPixel(29, 29), kRed));
  CHECK_NEAR(out.getPixel(0, 0).a, 0.f, 1e-5f);
  CHECK_NEAR(out.getPixel(30, 30).a, 0.f, 1e-5f);

  cmd.undo();
  CHECK_EQ(doc.width(), 100);
  CHECK_EQ(doc.tree().at(0)->originX, 40);
  CHECK_EQ(doc.tree().at(0)->originY, 40);
  auto* restored = dynamic_cast<PixelLayer*>(doc.tree().at(0));
  CHECK_EQ(restored->image.width(), 20);
  CHECK_EQ(restored->image.height(), 20);
}

TEST(crop_partial_overlap_shrinks_layer_to_overlap_rect) {
  Document doc(100, 100);
  // Layer content rect [20..50) × [20..50) (30×30 green).
  auto layer = solid(30, 30, kGreen, 1);
  layer->originX = 20;
  layer->originY = 20;
  doc.tree().add(std::move(layer));
  doc.setActiveLayerIndex(0);

  // Crop [35..70) × [35..70) — overlap is [35..50) × [35..50), 15×15.
  CropCommand cmd(&doc, Rect{35, 35, 35, 35});
  auto* pxl = dynamic_cast<PixelLayer*>(doc.tree().at(0));
  CHECK_EQ(pxl->image.width(), 15);
  CHECK_EQ(pxl->image.height(), 15);
  CHECK_EQ(pxl->originX, 0);  // overlap top-left == crop top-left
  CHECK_EQ(pxl->originY, 0);
  CHECK(approxEqual(pxl->image.getPixel(0, 0), kGreen));
  CHECK(approxEqual(pxl->image.getPixel(14, 14), kGreen));
}

TEST(crop_drops_layer_to_empty_when_no_overlap) {
  Document doc(100, 100);
  auto layer = solid(20, 20, kRed, 1);
  layer->originX = 0;
  layer->originY = 0;
  // Mask present so we exercise the mask-drop path.
  layer->mask = std::make_unique<LayerMask>(20, 20);
  layer->mask->image.fill(Rgba32F(1.f, 1.f, 1.f, 1.f));
  doc.tree().add(std::move(layer));
  doc.setActiveLayerIndex(0);

  // Crop far from the layer — no overlap.
  CropCommand cmd(&doc, Rect{50, 50, 20, 20});
  auto* pxl = dynamic_cast<PixelLayer*>(doc.tree().at(0));
  CHECK_EQ(pxl->image.width(), 0);
  CHECK_EQ(pxl->image.height(), 0);
  CHECK(pxl->mask == nullptr);

  // Composite should be all transparent.
  TuxImage out(20, 20);
  compose(doc.tree(), out);
  CHECK_NEAR(out.getPixel(0, 0).a, 0.f, 1e-5f);
  CHECK_NEAR(out.getPixel(19, 19).a, 0.f, 1e-5f);

  // Undo restores the layer and its mask.
  cmd.undo();
  auto* restored = dynamic_cast<PixelLayer*>(doc.tree().at(0));
  CHECK_EQ(restored->image.width(), 20);
  CHECK_EQ(restored->image.height(), 20);
  CHECK(restored->mask != nullptr);
}

TEST(document_addPixelLayer_installs_origin) {
  Document doc(100, 100);
  TuxImage img(30, 30);
  img.fill(kBlue);
  auto* layer = doc.addPixelLayer(std::move(img), 40, 50, "Placed");
  CHECK(layer != nullptr);
  CHECK_EQ(layer->originX, 40);
  CHECK_EQ(layer->originY, 50);
  CHECK_EQ(layer->image.width(), 30);
  CHECK_EQ(layer->image.height(), 30);

  TuxImage out(100, 100);
  compose(doc.tree(), out);
  CHECK(approxEqual(out.getPixel(40, 50), kBlue));
  CHECK(approxEqual(out.getPixel(69, 79), kBlue));
  CHECK_NEAR(out.getPixel(70, 80).a, 0.f, 1e-5f);
}

int main() { return tuxels::testing::run(); }
