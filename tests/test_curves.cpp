#include <cmath>
#include <memory>
#include <vector>

#include "compositor/compose.h"
#include "core/Pixel.h"
#include "core/TuxImage.h"
#include "geom/Spline.h"
#include "history/LayerParamsCommand.h"
#include "layers/CurvesAdjustment.h"
#include "layers/LayerMask.h"
#include "layers/LayerTree.h"
#include "layers/PixelLayer.h"
#include "test_harness.h"

using namespace tuxels;

namespace {

constexpr Rgba32F kGray50{0.5f, 0.5f, 0.5f, 1.f};
constexpr Rgba32F kWhite{1.f, 1.f, 1.f, 1.f};

std::unique_ptr<PixelLayer> solidPixelLayer(int w, int h, Rgba32F c,
                                            LayerId id) {
  auto l = std::make_unique<PixelLayer>(w, h);
  l->id = id;
  l->image.fill(c);
  return l;
}

Rgba32F composeWithCurves(Rgba32F base, std::unique_ptr<CurvesAdjustment> adj,
                          int sx = 8, int sy = 8) {
  LayerTree tree;
  tree.add(solidPixelLayer(32, 32, base, 1));
  adj->id = 2;
  tree.add(std::move(adj));
  TuxImage out(32, 32);
  compose(tree, out);
  return out.getPixel(sx, sy);
}

}  // namespace

TEST(curves_identity_preserves_composite) {
  // Default-constructed CurvesAdjustment has [(0,0),(1,1)] on every channel,
  // so a red base should round-trip bit-close through the LUT.
  auto adj = std::make_unique<CurvesAdjustment>();
  const Rgba32F p = composeWithCurves(Rgba32F{1.f, 0.f, 0.f, 1.f}, std::move(adj));
  CHECK_NEAR(p.r, 1.f, 1.f / 255.f);
  CHECK_NEAR(p.g, 0.f, 1.f / 255.f);
  CHECK_NEAR(p.b, 0.f, 1.f / 255.f);
  CHECK_NEAR(p.a, 1.f, 1e-5f);
}

TEST(curves_midpoint_lift_brightens_gray) {
  // Pull the composite midpoint to (0.5, 0.75) — the 0.5 gray sample
  // should brighten to ~0.75 (±2/255 for LUT quantisation).
  auto adj = std::make_unique<CurvesAdjustment>();
  std::vector<SplinePoint> pts = {
      {0.f, 0.f}, {0.5f, 0.75f}, {1.f, 1.f}};
  adj->setPoints(CurvesChannel::Composite, pts);
  const Rgba32F p = composeWithCurves(kGray50, std::move(adj));
  CHECK_NEAR(p.r, 0.75f, 3.f / 255.f);
  CHECK_NEAR(p.g, 0.75f, 3.f / 255.f);
  CHECK_NEAR(p.b, 0.75f, 3.f / 255.f);
}

TEST(curves_per_channel_does_not_disturb_other_channels) {
  // Pulling R's curve to zero must leave G and B untouched.
  auto adj = std::make_unique<CurvesAdjustment>();
  std::vector<SplinePoint> rPts = {{0.f, 0.f}, {1.f, 0.f}};
  adj->setPoints(CurvesChannel::R, rPts);
  const Rgba32F p = composeWithCurves(kWhite, std::move(adj));
  CHECK_NEAR(p.r, 0.f, 2.f / 255.f);
  CHECK_NEAR(p.g, 1.f, 2.f / 255.f);
  CHECK_NEAR(p.b, 1.f, 2.f / 255.f);
}

TEST(curves_mask_restricts_effect_to_revealed_region) {
  // Left half mask 0, right half 1 — with a "kill everything" curve the
  // right half should clamp to 0 while the left half stays white.
  LayerTree tree;
  tree.add(solidPixelLayer(32, 32, kWhite, 1));
  auto adj = std::make_unique<CurvesAdjustment>();
  adj->id = 2;
  adj->setPoints(CurvesChannel::Composite, {{0.f, 0.f}, {1.f, 0.f}});
  auto mask = std::make_unique<LayerMask>(32, 32);
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 32; ++x)
      mask->image.setPixel(x, y,
                           x < 16 ? Rgba32F{0.f, 0.f, 0.f, 1.f}
                                  : Rgba32F{1.f, 1.f, 1.f, 1.f});
  mask->enabled = true;
  adj->mask = std::move(mask);
  tree.add(std::move(adj));

  TuxImage out(32, 32);
  compose(tree, out);
  const Rgba32F left = out.getPixel(4, 16);
  const Rgba32F right = out.getPixel(24, 16);
  CHECK_NEAR(left.r, 1.f, 2.f / 255.f);
  CHECK_NEAR(right.r, 0.f, 2.f / 255.f);
}

TEST(curves_params_command_swaps_points) {
  auto layer = std::make_unique<CurvesAdjustment>();
  using PtArr = CurvesAdjustment::PointsArray;
  PtArr before = layer->allPoints();
  PtArr after = before;
  after[0] = {{0.f, 0.f}, {0.5f, 0.8f}, {1.f, 1.f}};

  auto setter = [](CurvesAdjustment* l, const PtArr& p) {
    l->setAllPoints(p);
  };
  LayerParamsCommand<CurvesAdjustment, PtArr> cmd(layer.get(), before, after,
                                                  setter, "Curves Edit");
  cmd.apply();
  CHECK_EQ(layer->points(CurvesChannel::Composite).size(), 3u);
  CHECK_NEAR(layer->points(CurvesChannel::Composite)[1].y, 0.8f, 1e-5f);
  cmd.undo();
  CHECK_EQ(layer->points(CurvesChannel::Composite).size(), 2u);
  cmd.redo();
  CHECK_EQ(layer->points(CurvesChannel::Composite).size(), 3u);
}

int main() { return ::tuxels::testing::run(); }
