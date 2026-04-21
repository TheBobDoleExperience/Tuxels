#include <cmath>
#include <memory>

#include "compositor/compose.h"
#include "core/Pixel.h"
#include "core/TuxImage.h"
#include "history/LayerParamsCommand.h"
#include "layers/HueSaturation.h"
#include "layers/LayerMask.h"
#include "layers/LayerTree.h"
#include "layers/PixelLayer.h"
#include "test_harness.h"

using namespace tuxels;

namespace {

constexpr Rgba32F kRed{1.f, 0.f, 0.f, 1.f};
constexpr Rgba32F kGray50{0.5f, 0.5f, 0.5f, 1.f};

std::unique_ptr<PixelLayer> solidPixelLayer(int w, int h, Rgba32F c,
                                            LayerId id) {
  auto l = std::make_unique<PixelLayer>(w, h);
  l->id = id;
  l->image.fill(c);
  return l;
}

Rgba32F composeSingleAdjustment(Rgba32F baseColor,
                                std::unique_ptr<HueSaturation> adj,
                                int sampleX = 8, int sampleY = 8) {
  LayerTree tree;
  tree.add(solidPixelLayer(32, 32, baseColor, 1));
  adj->id = 2;
  tree.add(std::move(adj));
  TuxImage out(32, 32);
  compose(tree, out);
  return out.getPixel(sampleX, sampleY);
}

}  // namespace

TEST(hue_saturation_identity_is_identity) {
  // Default {0, 0, 0} must round-trip red through rgbToHsl / hslToRgb
  // exactly — the identity path shouldn't introduce drift.
  auto adj = std::make_unique<HueSaturation>();
  const Rgba32F p = composeSingleAdjustment(kRed, std::move(adj));
  CHECK_NEAR(p.r, 1.f, 1e-5f);
  CHECK_NEAR(p.g, 0.f, 1e-5f);
  CHECK_NEAR(p.b, 0.f, 1e-5f);
  CHECK_NEAR(p.a, 1.f, 1e-5f);
}

TEST(hue_saturation_pure_red_hue_shift_120_is_pure_green) {
  // Rotating red (h=0°) by +120° lands on green in HSL.
  auto adj = std::make_unique<HueSaturation>();
  adj->setParams({120.f, 0.f, 0.f});
  const Rgba32F p = composeSingleAdjustment(kRed, std::move(adj));
  CHECK_NEAR(p.r, 0.f, 1e-4f);
  CHECK_NEAR(p.g, 1.f, 1e-4f);
  CHECK_NEAR(p.b, 0.f, 1e-4f);
}

TEST(hue_saturation_pure_red_hue_shift_240_is_pure_blue) {
  auto adj = std::make_unique<HueSaturation>();
  adj->setParams({240.f, 0.f, 0.f});
  const Rgba32F p = composeSingleAdjustment(kRed, std::move(adj));
  CHECK_NEAR(p.r, 0.f, 1e-4f);
  CHECK_NEAR(p.g, 0.f, 1e-4f);
  CHECK_NEAR(p.b, 1.f, 1e-4f);
}

TEST(hue_saturation_saturation_negative_one_desaturates_to_lightness) {
  // Pure red has l=0.5 in HSL. Dropping saturation to 0 returns
  // (l, l, l) — i.e. 50% gray.
  auto adj = std::make_unique<HueSaturation>();
  adj->setParams({0.f, -1.f, 0.f});
  const Rgba32F p = composeSingleAdjustment(kRed, std::move(adj));
  CHECK_NEAR(p.r, 0.5f, 1e-4f);
  CHECK_NEAR(p.g, 0.5f, 1e-4f);
  CHECK_NEAR(p.b, 0.5f, 1e-4f);
}

TEST(hue_saturation_lightness_positive_brightens) {
  // Mid-gray (l=0.5) + lightness=+0.2 → l=0.7, s=0 → (0.7, 0.7, 0.7).
  auto adj = std::make_unique<HueSaturation>();
  adj->setParams({0.f, 0.f, 0.2f});
  const Rgba32F p = composeSingleAdjustment(kGray50, std::move(adj));
  CHECK_NEAR(p.r, 0.7f, 1e-4f);
  CHECK_NEAR(p.g, 0.7f, 1e-4f);
  CHECK_NEAR(p.b, 0.7f, 1e-4f);
}

TEST(hue_saturation_alpha_is_untouched) {
  // Non-unit alpha on the base must pass through to the composite.
  auto adj = std::make_unique<HueSaturation>();
  adj->setParams({90.f, 0.f, 0.f});
  const Rgba32F base{1.f, 0.f, 0.f, 0.6f};
  const Rgba32F p = composeSingleAdjustment(base, std::move(adj));
  CHECK_NEAR(p.a, 0.6f, 1e-5f);
}

TEST(hue_saturation_mask_restricts_effect_to_revealed_region) {
  LayerTree tree;
  tree.add(solidPixelLayer(32, 32, kRed, 1));
  auto adj = std::make_unique<HueSaturation>();
  adj->id = 2;
  adj->setParams({120.f, 0.f, 0.f});

  auto mask = std::make_unique<LayerMask>(32, 32);
  for (int y = 0; y < 32; ++y) {
    for (int x = 0; x < 32; ++x) {
      mask->image.setPixel(x, y,
                           x < 16 ? Rgba32F{0.f, 0.f, 0.f, 1.f}
                                  : Rgba32F{1.f, 1.f, 1.f, 1.f});
    }
  }
  mask->enabled = true;
  adj->mask = std::move(mask);
  tree.add(std::move(adj));

  TuxImage out(32, 32);
  compose(tree, out);
  const Rgba32F left = out.getPixel(4, 16);
  const Rgba32F right = out.getPixel(24, 16);
  // Left half: mask=0, composite keeps the underlying red.
  CHECK_NEAR(left.r, 1.f, 1e-4f);
  CHECK_NEAR(left.g, 0.f, 1e-4f);
  // Right half: mask=1, red rotates to green.
  CHECK_NEAR(right.r, 0.f, 1e-4f);
  CHECK_NEAR(right.g, 1.f, 1e-4f);
}

TEST(hue_saturation_params_command_round_trip) {
  auto layer = std::make_unique<HueSaturation>();
  HueSaturationParams before = layer->params();
  HueSaturationParams after{60.f, 0.25f, -0.1f};

  auto setter = [](HueSaturation* l, const HueSaturationParams& p) {
    l->setParams(p);
  };
  LayerParamsCommand<HueSaturation, HueSaturationParams> cmd(
      layer.get(), before, after, setter, "Edit Hue/Saturation");
  cmd.apply();
  CHECK_NEAR(layer->params().hueShift, 60.f, 1e-6f);
  CHECK_NEAR(layer->params().saturation, 0.25f, 1e-6f);
  CHECK_NEAR(layer->params().lightness, -0.1f, 1e-6f);
  cmd.undo();
  CHECK_NEAR(layer->params().hueShift, 0.f, 1e-6f);
  CHECK_NEAR(layer->params().saturation, 0.f, 1e-6f);
  CHECK_NEAR(layer->params().lightness, 0.f, 1e-6f);
  cmd.redo();
  CHECK_NEAR(layer->params().hueShift, 60.f, 1e-6f);
}

int main() { return ::tuxels::testing::run(); }
