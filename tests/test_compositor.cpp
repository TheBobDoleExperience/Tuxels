#include <memory>

#include "compositor/compose.h"
#include "core/TuxImage.h"
#include "layers/LayerTree.h"
#include "layers/PixelLayer.h"
#include "test_harness.h"

using namespace tuxels;

static std::unique_ptr<PixelLayer> solidLayer(int w, int h, Rgba32F color,
                                              LayerId id) {
  auto l = std::make_unique<PixelLayer>(w, h);
  l->id = id;
  l->image.fill(color);
  return l;
}

TEST(empty_tree_produces_transparent_output) {
  LayerTree tree;
  TuxImage out(64, 64);
  compose(tree, out);
  CHECK_EQ(out.getPixel(0, 0), Rgba32F::transparent());
  CHECK_EQ(out.getPixel(63, 63), Rgba32F::transparent());
}

TEST(single_opaque_layer_copies_through) {
  LayerTree tree;
  tree.add(solidLayer(64, 64, Rgba32F(0.3f, 0.6f, 0.9f, 1.f), 1));
  TuxImage out(64, 64);
  compose(tree, out);
  CHECK(approxEqual(out.getPixel(0, 0), Rgba32F(0.3f, 0.6f, 0.9f, 1.f)));
  CHECK(approxEqual(out.getPixel(63, 63), Rgba32F(0.3f, 0.6f, 0.9f, 1.f)));
}

TEST(two_layers_half_opacity_normal_blends) {
  LayerTree tree;
  tree.add(solidLayer(32, 32, Rgba32F(1.f, 0.f, 0.f, 1.f), 1));  // red bottom
  auto top = solidLayer(32, 32, Rgba32F(0.f, 1.f, 0.f, 1.f), 2);  // green top
  top->opacity = 0.5f;
  tree.add(std::move(top));
  TuxImage out(32, 32);
  compose(tree, out);
  auto p = out.getPixel(10, 10);
  CHECK(approxEqual(p, Rgba32F(0.5f, 0.5f, 0.f, 1.f), 1e-4f));
}

TEST(multiply_blend_darkens) {
  LayerTree tree;
  tree.add(solidLayer(16, 16, Rgba32F(0.5f, 0.5f, 0.5f, 1.f), 1));
  auto top = solidLayer(16, 16, Rgba32F(0.5f, 0.5f, 0.5f, 1.f), 2);
  top->blend = BlendMode::Multiply;
  tree.add(std::move(top));
  TuxImage out(16, 16);
  compose(tree, out);
  auto p = out.getPixel(5, 5);
  CHECK(approxEqual(p, Rgba32F(0.25f, 0.25f, 0.25f, 1.f), 1e-4f));
}

TEST(invisible_layer_is_ignored) {
  LayerTree tree;
  tree.add(solidLayer(16, 16, Rgba32F(0.25f, 0.25f, 0.25f, 1.f), 1));
  auto top = solidLayer(16, 16, Rgba32F(1.f, 1.f, 1.f, 1.f), 2);
  top->visible = false;
  tree.add(std::move(top));
  TuxImage out(16, 16);
  compose(tree, out);
  CHECK(approxEqual(out.getPixel(0, 0), Rgba32F(0.25f, 0.25f, 0.25f, 1.f)));
}

TEST(mask_hides_fully_when_zero) {
  LayerTree tree;
  tree.add(solidLayer(16, 16, Rgba32F(1.f, 0.f, 0.f, 1.f), 1));  // red bottom
  auto top = solidLayer(16, 16, Rgba32F(0.f, 1.f, 0.f, 1.f), 2);  // green top
  top->mask = std::make_unique<LayerMask>(16, 16);
  top->mask->image.fill(Rgba32F(0.f, 0.f, 0.f, 1.f));  // mask=0
  tree.add(std::move(top));
  TuxImage out(16, 16);
  compose(tree, out);
  CHECK(approxEqual(out.getPixel(0, 0), Rgba32F(1.f, 0.f, 0.f, 1.f)));
}

TEST(mask_reveals_fully_when_one) {
  LayerTree tree;
  tree.add(solidLayer(16, 16, Rgba32F(1.f, 0.f, 0.f, 1.f), 1));
  auto top = solidLayer(16, 16, Rgba32F(0.f, 1.f, 0.f, 1.f), 2);
  top->mask = std::make_unique<LayerMask>(16, 16);
  top->mask->image.fill(Rgba32F(1.f, 1.f, 1.f, 1.f));
  tree.add(std::move(top));
  TuxImage out(16, 16);
  compose(tree, out);
  CHECK(approxEqual(out.getPixel(0, 0), Rgba32F(0.f, 1.f, 0.f, 1.f)));
}

TEST(disabled_mask_is_bypassed) {
  LayerTree tree;
  tree.add(solidLayer(16, 16, Rgba32F(1.f, 0.f, 0.f, 1.f), 1));
  auto top = solidLayer(16, 16, Rgba32F(0.f, 1.f, 0.f, 1.f), 2);
  top->mask = std::make_unique<LayerMask>(16, 16);
  top->mask->image.fill(Rgba32F(0.f, 0.f, 0.f, 1.f));  // would hide
  top->mask->enabled = false;  // but disabled
  tree.add(std::move(top));
  TuxImage out(16, 16);
  compose(tree, out);
  CHECK(approxEqual(out.getPixel(0, 0), Rgba32F(0.f, 1.f, 0.f, 1.f)));
}

TEST(mask_half_intensity_is_linear_mix) {
  LayerTree tree;
  tree.add(solidLayer(16, 16, Rgba32F(1.f, 0.f, 0.f, 1.f), 1));
  auto top = solidLayer(16, 16, Rgba32F(0.f, 1.f, 0.f, 1.f), 2);
  top->mask = std::make_unique<LayerMask>(16, 16);
  top->mask->image.fill(Rgba32F(0.5f, 0.5f, 0.5f, 1.f));
  tree.add(std::move(top));
  TuxImage out(16, 16);
  compose(tree, out);
  CHECK(approxEqual(out.getPixel(0, 0), Rgba32F(0.5f, 0.5f, 0.f, 1.f), 1e-4f));
}

TEST(reorder_layers_changes_composite) {
  LayerTree tree;
  auto a = solidLayer(16, 16, Rgba32F(1.f, 0.f, 0.f, 1.f), 1);
  auto b = solidLayer(16, 16, Rgba32F(0.f, 1.f, 0.f, 1.f), 2);
  tree.add(std::move(a));
  tree.add(std::move(b));
  TuxImage out(16, 16);
  compose(tree, out);
  CHECK(approxEqual(out.getPixel(0, 0), Rgba32F(0.f, 1.f, 0.f, 1.f)));
  tree.move(0, 1);  // a→top
  compose(tree, out);
  CHECK(approxEqual(out.getPixel(0, 0), Rgba32F(1.f, 0.f, 0.f, 1.f)));
}

TEST(tiles_at_edge_are_cropped_and_transparent_outside) {
  LayerTree tree;
  // Layer fills only part of the full tile span of the output image.
  tree.add(solidLayer(300, 300, Rgba32F(0.5f, 0.5f, 0.5f, 1.f), 1));
  TuxImage out(300, 300);  // 2x2 tiles
  compose(tree, out);
  CHECK(approxEqual(out.getPixel(299, 299), Rgba32F(0.5f, 0.5f, 0.5f, 1.f)));
  // Outside the image we read transparent (inBounds guard).
  CHECK(approxEqual(out.getPixel(300, 300), Rgba32F::transparent()));
}

TEST(partial_compose_updates_only_covered_tiles) {
  // 600×600 → 3×3 tiles of 256px. Start with a single red layer. Full
  // compose fills everything red. Swap the layer for green and partial-
  // compose only the tile-(0,0) region; the other tiles must still read
  // red from the previous composite.
  LayerTree tree;
  tree.add(solidLayer(600, 600, Rgba32F(1.f, 0.f, 0.f, 1.f), 1));
  TuxImage out(600, 600);
  compose(tree, out);
  CHECK(approxEqual(out.getPixel(0, 0), Rgba32F(1.f, 0.f, 0.f, 1.f)));
  CHECK(approxEqual(out.getPixel(500, 500), Rgba32F(1.f, 0.f, 0.f, 1.f)));

  tree.removeAt(0);
  tree.add(solidLayer(600, 600, Rgba32F(0.f, 1.f, 0.f, 1.f), 2));
  compose(tree, out, Rect{10, 10, 40, 40});  // lives in tile (0,0)
  CHECK(approxEqual(out.getPixel(20, 20), Rgba32F(0.f, 1.f, 0.f, 1.f)));
  // Tile (2,2) was not re-composed — still the previous red.
  CHECK(approxEqual(out.getPixel(500, 500), Rgba32F(1.f, 0.f, 0.f, 1.f)));
}

TEST(partial_compose_empty_rect_is_no_op) {
  LayerTree tree;
  tree.add(solidLayer(64, 64, Rgba32F(1.f, 0.f, 0.f, 1.f), 1));
  TuxImage out(64, 64);
  compose(tree, out, Rect{0, 0, 0, 0});
  CHECK_EQ(out.getPixel(0, 0), Rgba32F::transparent());
}

TEST(partial_compose_crosses_tile_boundary) {
  LayerTree tree;
  tree.add(solidLayer(600, 600, Rgba32F(0.25f, 0.25f, 0.25f, 1.f), 1));
  TuxImage out(600, 600);
  compose(tree, out, Rect{250, 250, 20, 20});  // spans tile (0,0) and (1,1)
  CHECK(approxEqual(out.getPixel(255, 255), Rgba32F(0.25f, 0.25f, 0.25f, 1.f)));
  CHECK(approxEqual(out.getPixel(260, 260), Rgba32F(0.25f, 0.25f, 0.25f, 1.f)));
}

int main() { return tuxels::testing::run(); }
