#include <memory>
#include <utility>

#include "compositor/compose.h"
#include "core/Document.h"
#include "core/Pixel.h"
#include "core/TuxImage.h"
#include "history/TransformCommand.h"
#include "layers/PixelLayer.h"
#include "test_harness.h"

using namespace tuxels;

namespace {

constexpr Rgba32F kRed{1.f, 0.f, 0.f, 1.f};
constexpr Rgba32F kGreen{0.f, 1.f, 0.f, 1.f};
constexpr Rgba32F kBlue{0.f, 0.f, 1.f, 1.f};

PixelLayer* addSolidLayer(Document& doc, int w, int h, Rgba32F c, int ox,
                          int oy, const char* name) {
  TuxImage img(w, h);
  img.fill(c);
  return doc.addPixelLayer(std::move(img), ox, oy, name);
}

}  // namespace

TEST(transform_command_redo_installs_after_image_and_origin) {
  Document doc(100, 100);
  auto* layer = addSolidLayer(doc, 20, 20, kRed, 10, 10, "Layer");
  const LayerId id = layer->id;

  TuxImage beforeImg = layer->image;  // red, 20×20
  TuxImage afterImg(30, 30);
  afterImg.fill(kBlue);

  TransformCommand cmd(&doc, id, beforeImg, afterImg, 10, 10, 25, 35);
  cmd.redo();

  CHECK_EQ(layer->originX, 25);
  CHECK_EQ(layer->originY, 35);
  CHECK_EQ(layer->image.width(), 30);
  CHECK_EQ(layer->image.height(), 30);
  CHECK(approxEqual(layer->image.getPixel(0, 0), kBlue));
  CHECK(approxEqual(layer->image.getPixel(29, 29), kBlue));
}

TEST(transform_command_undo_restores_before_image_and_origin) {
  Document doc(100, 100);
  auto* layer = addSolidLayer(doc, 20, 20, kRed, 10, 10, "Layer");
  const LayerId id = layer->id;

  TuxImage beforeImg = layer->image;
  TuxImage afterImg(30, 30);
  afterImg.fill(kBlue);

  TransformCommand cmd(&doc, id, beforeImg, afterImg, 10, 10, 25, 35);
  cmd.redo();
  cmd.undo();

  CHECK_EQ(layer->originX, 10);
  CHECK_EQ(layer->originY, 10);
  CHECK_EQ(layer->image.width(), 20);
  CHECK_EQ(layer->image.height(), 20);
  CHECK(approxEqual(layer->image.getPixel(5, 5), kRed));
}

TEST(transform_command_round_trip_survives_tree_reorder) {
  // Id-based lookup must keep routing to the right layer even if the tree
  // is shuffled between commit and the subsequent undo/redo.
  Document doc(100, 100);
  auto* a = addSolidLayer(doc, 10, 10, kRed, 0, 0, "A");
  auto* b = addSolidLayer(doc, 10, 10, kGreen, 50, 50, "B");

  TuxImage beforeImg = b->image;
  TuxImage afterImg(10, 10);
  afterImg.fill(kBlue);

  TransformCommand cmd(&doc, b->id, beforeImg, afterImg, 50, 50, 60, 70);
  cmd.redo();  // b becomes blue at (60, 70)

  // Swap layer order: B now at index 0, A at index 1.
  doc.tree().move(1, 0);
  CHECK_EQ(doc.tree().at(0), b);
  CHECK_EQ(doc.tree().at(1), a);

  cmd.undo();  // Must still reach b via its id, not via old index.
  CHECK_EQ(b->originX, 50);
  CHECK_EQ(b->originY, 50);
  CHECK(approxEqual(b->image.getPixel(3, 3), kGreen));
  CHECK_EQ(a->originX, 0);  // A untouched.
}

TEST(transform_command_propagates_through_composite) {
  // Commit a transform and check that compose() reflects the new image +
  // origin. Sanity for the full pipeline: command swap + doc-space render.
  Document doc(100, 100);
  auto* bg = addSolidLayer(doc, 100, 100, kRed, 0, 0, "BG");
  auto* layer = addSolidLayer(doc, 20, 20, kGreen, 10, 10, "Layer");
  (void)bg;

  TuxImage beforeImg = layer->image;
  TuxImage afterImg(20, 20);
  afterImg.fill(kBlue);

  TransformCommand cmd(&doc, layer->id, beforeImg, afterImg, 10, 10, 40, 40);
  cmd.redo();

  TuxImage out(100, 100);
  compose(doc.tree(), out);
  // Blue at the new origin.
  CHECK(approxEqual(out.getPixel(45, 45), kBlue));
  // Red where the green block used to be.
  CHECK(approxEqual(out.getPixel(15, 15), kRed));

  cmd.undo();
  compose(doc.tree(), out);
  CHECK(approxEqual(out.getPixel(15, 15), kGreen));
  CHECK(approxEqual(out.getPixel(45, 45), kRed));
}

int main() { return tuxels::testing::run(); }
