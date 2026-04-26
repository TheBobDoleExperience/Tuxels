#include <memory>

#include "compositor/compose.h"
#include "core/Document.h"
#include "core/Pixel.h"
#include "core/TuxImage.h"
#include "layers/AdjustmentLayer.h"
#include "layers/LayerTree.h"
#include "layers/PixelLayer.h"
#include "test_harness.h"

using namespace tuxels;

namespace {

constexpr Rgba32F kRed{1.f, 0.f, 0.f, 1.f};
constexpr Rgba32F kGreen{0.f, 1.f, 0.f, 1.f};
constexpr Rgba32F kCyan{0.f, 1.f, 1.f, 1.f};
constexpr Rgba32F kMagenta{1.f, 0.f, 1.f, 1.f};
constexpr Rgba32F kTransparent{0.f, 0.f, 0.f, 0.f};

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

// Fill a doc-sized layer's left half with `c`, leave right half transparent.
std::unique_ptr<PixelLayer> leftHalfPixelLayer(int w, int h, Rgba32F c,
                                               LayerId id) {
  auto l = std::make_unique<PixelLayer>(w, h);
  l->id = id;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w / 2; ++x) {
      l->image.setPixel(x, y, c);
    }
  }
  return l;
}

}  // namespace

// Without clipping the invert leaks past the green sprite onto the red
// background — left half gets cyan (invert of red+green-over-red blend
// flow gives invert-of-green = magenta on the left, since green covers it),
// right half is the invert of red = cyan.
//
// Wait — restate carefully: bottom red, top green-on-left half, then
// invert. Composite below invert is green on left, red on right. Invert
// gives magenta on left, cyan on right. With clipping to the green sprite,
// left stays magenta (green's alpha = 1 there), right reverts to red
// (green's alpha = 0 there).
TEST(unclipped_adjustment_affects_full_doc) {
  LayerTree tree;
  tree.add(solidPixelLayer(32, 32, kRed, 1));
  tree.add(leftHalfPixelLayer(32, 32, kGreen, 2));
  auto inv = std::make_unique<InvertAdjustment>();
  inv->id = 3;
  inv->clipToBelow = false;
  tree.add(std::move(inv));

  TuxImage out(32, 32);
  compose(tree, out);

  CHECK(approxEqual(out.getPixel(4, 16), kMagenta));   // invert of green
  CHECK(approxEqual(out.getPixel(20, 16), kCyan));     // invert of red
}

TEST(clipped_adjustment_is_gated_by_base_alpha) {
  LayerTree tree;
  tree.add(solidPixelLayer(32, 32, kRed, 1));
  tree.add(leftHalfPixelLayer(32, 32, kGreen, 2));
  auto inv = std::make_unique<InvertAdjustment>();
  inv->id = 3;
  inv->clipToBelow = true;  // clip to immediate base = green sprite
  tree.add(std::move(inv));

  TuxImage out(32, 32);
  compose(tree, out);

  // Left half: green's alpha = 1, so invert applies → magenta.
  CHECK(approxEqual(out.getPixel(4, 16), kMagenta));
  // Right half: green's alpha = 0, so invert gated to factor 0 → red
  // (the red bg from below) is what shows through.
  CHECK(approxEqual(out.getPixel(20, 16), kRed));
}

TEST(two_adjacent_clipped_adjustments_share_one_base) {
  // Stack: red bg / green-left-half / clipped invert / clipped invert.
  // Two inverts compose to identity, so the visible result on the green
  // half should be green (not magenta). Right half stays red because both
  // are gated by green's alpha.
  LayerTree tree;
  tree.add(solidPixelLayer(32, 32, kRed, 1));
  tree.add(leftHalfPixelLayer(32, 32, kGreen, 2));
  auto inv1 = std::make_unique<InvertAdjustment>();
  inv1->id = 3;
  inv1->clipToBelow = true;
  tree.add(std::move(inv1));
  auto inv2 = std::make_unique<InvertAdjustment>();
  inv2->id = 4;
  inv2->clipToBelow = true;
  tree.add(std::move(inv2));

  TuxImage out(32, 32);
  compose(tree, out);

  // Left half: invert(invert(green)) = green.
  CHECK(approxEqual(out.getPixel(4, 16), kGreen));
  // Right half: gated to factor 0 by green's alpha, both inverts collapse
  // to no-ops, red shows through.
  CHECK(approxEqual(out.getPixel(20, 16), kRed));
}

TEST(clipped_adjustment_with_no_base_below_is_noop) {
  // Adjustment layer is the bottom of the tree, marked clipped — there's
  // no preceding pixel layer to gate against, so the adjustment is
  // skipped entirely. Composite is empty (nothing to invert).
  LayerTree tree;
  auto inv = std::make_unique<InvertAdjustment>();
  inv->id = 1;
  inv->clipToBelow = true;
  tree.add(std::move(inv));

  TuxImage out(32, 32);
  compose(tree, out);

  CHECK(approxEqual(out.getPixel(4, 16), kTransparent));
}

TEST(unclipped_adjustment_does_not_become_a_base) {
  // Stack: red bg / unclipped invert / green-left-half / clipped invert.
  // The middle adjustment shouldn't reset the base — the clipped invert
  // at the top should clip to the green sprite, NOT to the inverted red
  // composite below it. So left half = invert(green) = magenta;
  // right half = invert(red) = cyan (the unclipped middle invert leaks
  // past green's alpha-zero region).
  LayerTree tree;
  tree.add(solidPixelLayer(32, 32, kRed, 1));
  auto invMid = std::make_unique<InvertAdjustment>();
  invMid->id = 2;
  invMid->clipToBelow = false;
  tree.add(std::move(invMid));
  tree.add(leftHalfPixelLayer(32, 32, kGreen, 3));
  auto invTop = std::make_unique<InvertAdjustment>();
  invTop->id = 4;
  invTop->clipToBelow = true;
  tree.add(std::move(invTop));

  TuxImage out(32, 32);
  compose(tree, out);

  // Left half: green covers the inverted red below it, then top invert
  // clips to green's alpha=1 → invert applied → magenta.
  CHECK(approxEqual(out.getPixel(4, 16), kMagenta));
  // Right half: green's alpha=0, top invert gated out, the unclipped
  // middle invert's effect on red shows = cyan.
  CHECK(approxEqual(out.getPixel(20, 16), kCyan));
}

int main() { return tuxels::testing::run(); }
