#include <cmath>
#include <memory>

#include "compositor/compose.h"
#include "core/Pixel.h"
#include "core/TuxImage.h"
#include "history/LayerParamsCommand.h"
#include "layers/BrightnessContrast.h"
#include "layers/LayerMask.h"
#include "layers/LayerTree.h"
#include "layers/PixelLayer.h"
#include "test_harness.h"

using namespace tuxels;

namespace {

constexpr Rgba32F kRed{1.f, 0.f, 0.f, 1.f};
constexpr Rgba32F kGray25{0.25f, 0.25f, 0.25f, 1.f};
constexpr Rgba32F kGray50{0.5f, 0.5f, 0.5f, 1.f};
constexpr Rgba32F kGray75{0.75f, 0.75f, 0.75f, 1.f};

std::unique_ptr<PixelLayer> solidPixelLayer(int w, int h, Rgba32F c,
                                            LayerId id) {
  auto l = std::make_unique<PixelLayer>(w, h);
  l->id = id;
  l->image.fill(c);
  return l;
}

Rgba32F composeSingleAdjustment(Rgba32F baseColor,
                                std::unique_ptr<BrightnessContrast> adj,
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

TEST(brightness_contrast_identity_is_identity) {
  // Default-constructed params are {0, 0} — the LUT must be the 0..255
  // straight identity, and a red composite must survive intact within a
  // single byte of LUT quantisation.
  auto adj = std::make_unique<BrightnessContrast>();
  const uint8_t* lut = adj->lut();
  for (int i = 0; i < 256; ++i) CHECK_EQ(int(lut[i]), i);

  const Rgba32F p = composeSingleAdjustment(kRed, std::move(adj));
  CHECK_NEAR(p.r, 1.f, 1.f / 255.f);
  CHECK_NEAR(p.g, 0.f, 1.f / 255.f);
  CHECK_NEAR(p.b, 0.f, 1.f / 255.f);
  CHECK_NEAR(p.a, 1.f, 1e-5f);
}

TEST(brightness_contrast_pure_brightness_shifts_values) {
  // brightness=0.5 adds 0.5 uniformly — 0.25-gray rises to 0.75 (LUT
  // quantisation ±1/255).
  auto adj = std::make_unique<BrightnessContrast>();
  adj->setParams({0.5f, 0.f});
  const Rgba32F out = composeSingleAdjustment(kGray25, std::move(adj));
  CHECK_NEAR(out.r, 0.75f, 2.f / 255.f);
  CHECK_NEAR(out.g, 0.75f, 2.f / 255.f);
  CHECK_NEAR(out.b, 0.75f, 2.f / 255.f);
}

TEST(brightness_contrast_pure_contrast_stretches_around_midpoint) {
  // contrast=0.5 stretches by 1.5× around 0.5: 0.25 → 0.125, 0.75 → 0.875,
  // midpoint 0.5 unchanged.
  auto mk = []() {
    auto a = std::make_unique<BrightnessContrast>();
    a->setParams({0.f, 0.5f});
    return a;
  };
  const Rgba32F low = composeSingleAdjustment(kGray25, mk());
  CHECK_NEAR(low.r, 0.125f, 2.f / 255.f);
  const Rgba32F mid = composeSingleAdjustment(kGray50, mk());
  CHECK_NEAR(mid.r, 0.5f, 2.f / 255.f);
  const Rgba32F high = composeSingleAdjustment(kGray75, mk());
  CHECK_NEAR(high.r, 0.875f, 2.f / 255.f);
}

TEST(brightness_contrast_clamps_at_extremes) {
  // brightness=2 is out of the UI range but the LUT must still saturate
  // cleanly to 1.0 rather than overflow.
  auto adj = std::make_unique<BrightnessContrast>();
  adj->setParams({2.f, 0.f});
  const Rgba32F out = composeSingleAdjustment(kGray50, std::move(adj));
  CHECK_NEAR(out.r, 1.f, 1.f / 255.f);
  CHECK_NEAR(out.g, 1.f, 1.f / 255.f);
  CHECK_NEAR(out.b, 1.f, 1.f / 255.f);
}

TEST(brightness_contrast_mask_restricts_effect_to_revealed_region) {
  // Left-half mask = 0 leaves the base white intact; right-half mask = 1
  // applies brightness=-1, fully darkening.
  LayerTree tree;
  tree.add(solidPixelLayer(32, 32, Rgba32F{1.f, 1.f, 1.f, 1.f}, 1));
  auto adj = std::make_unique<BrightnessContrast>();
  adj->id = 2;
  adj->setParams({-1.f, 0.f});

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
  CHECK_NEAR(left.r, 1.f, 2.f / 255.f);
  CHECK_NEAR(right.r, 0.f, 2.f / 255.f);
}

TEST(brightness_contrast_params_command_round_trip) {
  // Apply/undo/redo through LayerParamsCommand swaps the cached LUT so the
  // live `params()` returns the correct struct at each step.
  auto layer = std::make_unique<BrightnessContrast>();
  BrightnessContrastParams before = layer->params();
  BrightnessContrastParams after{0.3f, -0.4f};

  auto setter = [](BrightnessContrast* l, const BrightnessContrastParams& p) {
    l->setParams(p);
  };
  LayerParamsCommand<BrightnessContrast, BrightnessContrastParams> cmd(
      layer.get(), before, after, setter, "Edit B/C");
  cmd.apply();
  CHECK_NEAR(layer->params().brightness, 0.3f, 1e-6f);
  CHECK_NEAR(layer->params().contrast, -0.4f, 1e-6f);
  cmd.undo();
  CHECK_NEAR(layer->params().brightness, 0.f, 1e-6f);
  CHECK_NEAR(layer->params().contrast, 0.f, 1e-6f);
  cmd.redo();
  CHECK_NEAR(layer->params().brightness, 0.3f, 1e-6f);
  CHECK_NEAR(layer->params().contrast, -0.4f, 1e-6f);
}

int main() { return ::tuxels::testing::run(); }
