#include <memory>

#include "compositor/compose.h"
#include "core/Document.h"
#include "core/Pixel.h"
#include "core/TuxImage.h"
#include "layers/AdjustmentLayer.h"
#include "layers/LayerMask.h"
#include "layers/LayerTree.h"
#include "layers/PixelLayer.h"
#include "test_harness.h"

using namespace tuxels;

namespace {

constexpr Rgba32F kRed{1.f, 0.f, 0.f, 1.f};
constexpr Rgba32F kCyan{0.f, 1.f, 1.f, 1.f};

// Test-only adjustment that negates RGB, leaves A. Exercises the compose
// adjustment-dispatch path, opacity blend, and mask factor without
// pulling in a real adjustment type (those ship in M3-S2 / S3).
class InvertAdjustment : public AdjustmentLayer {
 public:
  void applyToAccum(TileCoord /*tc*/, Rgba32F* accum) const override {
    for (int i = 0; i < kTilePixels; ++i) {
      Rgba32F& p = accum[i];
      p.r = 1.f - p.r;
      p.g = 1.f - p.g;
      p.b = 1.f - p.b;
    }
  }
};

std::unique_ptr<PixelLayer> solidPixelLayer(int w, int h, Rgba32F c,
                                            LayerId id) {
  auto l = std::make_unique<PixelLayer>(w, h);
  l->id = id;
  l->image.fill(c);
  return l;
}

}  // namespace

TEST(adjustment_kind_is_adjustment) {
  InvertAdjustment adj;
  CHECK(adj.kind() == LayerKind::Adjustment);
}

TEST(adjustment_renderTile_returns_false) {
  InvertAdjustment adj;
  Rgba32F tile[kTilePixels];
  CHECK(!adj.renderTile({0, 0}, tile));
}

TEST(adjustment_full_opacity_inverts_composite) {
  LayerTree tree;
  tree.add(solidPixelLayer(32, 32, kRed, 1));
  auto inv = std::make_unique<InvertAdjustment>();
  inv->id = 2;
  inv->opacity = 1.f;
  tree.add(std::move(inv));

  TuxImage out(32, 32);
  compose(tree, out);
  CHECK(approxEqual(out.getPixel(0, 0), kCyan));
  CHECK(approxEqual(out.getPixel(16, 16), kCyan));
}

TEST(adjustment_half_opacity_lerps_halfway) {
  LayerTree tree;
  tree.add(solidPixelLayer(32, 32, kRed, 1));
  auto inv = std::make_unique<InvertAdjustment>();
  inv->id = 2;
  inv->opacity = 0.5f;
  tree.add(std::move(inv));

  TuxImage out(32, 32);
  compose(tree, out);
  const Rgba32F p = out.getPixel(8, 8);
  CHECK_NEAR(p.r, 0.5f, 1e-5f);
  CHECK_NEAR(p.g, 0.5f, 1e-5f);
  CHECK_NEAR(p.b, 0.5f, 1e-5f);
  CHECK_NEAR(p.a, 1.f, 1e-5f);
}

TEST(adjustment_mask_confines_effect_to_right_half) {
  LayerTree tree;
  tree.add(solidPixelLayer(32, 32, kRed, 1));
  auto inv = std::make_unique<InvertAdjustment>();
  inv->id = 2;
  inv->opacity = 1.f;
  auto mask = std::make_unique<LayerMask>(32, 32);
  // Left half hides the adjustment (mask.r = 0); right half reveals it.
  for (int y = 0; y < 32; ++y) {
    for (int x = 0; x < 32; ++x) {
      mask->image.setPixel(x, y,
                           x < 16 ? Rgba32F{0.f, 0.f, 0.f, 1.f}
                                  : Rgba32F{1.f, 1.f, 1.f, 1.f});
    }
  }
  mask->enabled = true;
  inv->mask = std::move(mask);
  tree.add(std::move(inv));

  TuxImage out(32, 32);
  compose(tree, out);
  CHECK(approxEqual(out.getPixel(0, 0), kRed));
  CHECK(approxEqual(out.getPixel(15, 15), kRed));
  CHECK(approxEqual(out.getPixel(16, 16), kCyan));
  CHECK(approxEqual(out.getPixel(31, 31), kCyan));
}

TEST(adjustment_over_transparent_leaves_transparent) {
  // No pixel layer below — the adjustment has nothing to transform.
  // Compose skips accum pixels with a<=0 so the output stays transparent.
  LayerTree tree;
  auto inv = std::make_unique<InvertAdjustment>();
  inv->id = 1;
  inv->opacity = 1.f;
  tree.add(std::move(inv));

  TuxImage out(32, 32);
  compose(tree, out);
  CHECK_NEAR(out.getPixel(0, 0).a, 0.f, 1e-5f);
  CHECK_NEAR(out.getPixel(16, 16).a, 0.f, 1e-5f);
}

TEST(addAdjustmentLayer_auto_attaches_white_mask_and_flips_paint_target) {
  Document doc(32, 32);
  auto inv = std::make_unique<InvertAdjustment>();
  InvertAdjustment* raw = doc.addAdjustmentLayer(std::move(inv));
  CHECK(raw != nullptr);
  CHECK(raw->mask != nullptr);
  CHECK(raw->mask->enabled);
  CHECK_EQ(raw->mask->image.width(), 32);
  CHECK_EQ(raw->mask->image.height(), 32);
  CHECK_NEAR(raw->mask->image.getPixel(0, 0).r, 1.f, 1e-5f);
  CHECK_NEAR(raw->mask->image.getPixel(31, 31).r, 1.f, 1e-5f);
  // Paint target flips to Mask so follow-up brushwork lands on the
  // auto-mask by default.
  CHECK(doc.paintTarget() == PaintTarget::Mask);
}

int main() { return ::tuxels::testing::run(); }
