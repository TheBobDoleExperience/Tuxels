#include <array>
#include <cmath>
#include <memory>

#include "compositor/compose.h"
#include "core/Pixel.h"
#include "core/TuxImage.h"
#include "history/LayerParamsCommand.h"
#include "layers/LayerMask.h"
#include "layers/LayerTree.h"
#include "layers/LevelsAdjustment.h"
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
                                std::unique_ptr<LevelsAdjustment> adj,
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

TEST(levels_identity_params_produce_identity_lut) {
  LevelsAdjustment adj;
  // Default-constructed params are identity — the LUT must be 0..255 straight.
  for (int c = 0; c < 4; ++c) {
    const uint8_t* lut = adj.lut(static_cast<LevelsChannel>(c));
    for (int i = 0; i < 256; ++i) CHECK_EQ(int(lut[i]), i);
  }
}

TEST(levels_identity_params_preserve_composite) {
  // Identity levels layer over a constant-red base must leave the composite
  // bit-exact (within a single byte of LUT quantisation).
  auto adj = std::make_unique<LevelsAdjustment>();
  const Rgba32F p = composeSingleAdjustment(kRed, std::move(adj));
  CHECK_NEAR(p.r, 1.f, 1.f / 255.f);
  CHECK_NEAR(p.g, 0.f, 1.f / 255.f);
  CHECK_NEAR(p.b, 0.f, 1.f / 255.f);
  CHECK_NEAR(p.a, 1.f, 1e-5f);
}

TEST(levels_composite_gamma_brightens_midtones) {
  // gamma=2.2 on composite: a 0.5-gray input should brighten by the
  // standard pow(0.5, 1/2.2) ≈ 0.7297 (±1/255 for LUT quantisation).
  auto adj = std::make_unique<LevelsAdjustment>();
  LevelsParams p;
  p.gamma = 2.2f;
  adj->setParams(LevelsChannel::Composite, p);
  const float expected = std::pow(0.5f, 1.f / 2.2f);
  const Rgba32F out = composeSingleAdjustment(kGray50, std::move(adj));
  CHECK_NEAR(out.r, expected, 2.f / 255.f);
  CHECK_NEAR(out.g, expected, 2.f / 255.f);
  CHECK_NEAR(out.b, expected, 2.f / 255.f);
}

TEST(levels_inBlack_half_clips_lower_half_to_zero) {
  // inBlack=0.5 maps everything ≤ 0.5 to 0. A 0.5-gray input sits on the
  // boundary and should map to 0; a red (1.0) stays near 1.
  auto adj = std::make_unique<LevelsAdjustment>();
  LevelsParams p;
  p.inBlack = 0.5f;
  adj->setParams(LevelsChannel::Composite, p);
  const Rgba32F gray = composeSingleAdjustment(kGray50, std::move(adj));
  CHECK_NEAR(gray.r, 0.f, 2.f / 255.f);
  CHECK_NEAR(gray.g, 0.f, 2.f / 255.f);
  CHECK_NEAR(gray.b, 0.f, 2.f / 255.f);
}

TEST(levels_per_channel_does_not_disturb_other_channels) {
  // Pull red's output white down to 0 — G and B must be unaffected.
  auto adj = std::make_unique<LevelsAdjustment>();
  LevelsParams p;
  p.outWhite = 0.f;
  adj->setParams(LevelsChannel::R, p);

  LayerTree tree;
  tree.add(solidPixelLayer(32, 32, Rgba32F{1.f, 1.f, 1.f, 1.f}, 1));
  adj->id = 2;
  tree.add(std::move(adj));
  TuxImage out(32, 32);
  compose(tree, out);
  const Rgba32F px = out.getPixel(8, 8);
  CHECK_NEAR(px.r, 0.f, 2.f / 255.f);
  CHECK_NEAR(px.g, 1.f, 2.f / 255.f);
  CHECK_NEAR(px.b, 1.f, 2.f / 255.f);
}

TEST(levels_mask_restricts_effect_to_revealed_region) {
  // Mask left half to 0 → left half is unaffected; right half inverts to 0.
  LayerTree tree;
  tree.add(solidPixelLayer(32, 32, Rgba32F{1.f, 1.f, 1.f, 1.f}, 1));
  auto adj = std::make_unique<LevelsAdjustment>();
  adj->id = 2;
  LevelsParams p;
  p.outWhite = 0.f;
  adj->setParams(LevelsChannel::Composite, p);

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

TEST(levels_params_command_swaps_params) {
  // Round-trip through a LayerParamsCommand: apply moves to `after`,
  // undo restores `before`.
  auto layer = std::make_unique<LevelsAdjustment>();
  using ParamArr = std::array<LevelsParams, 4>;
  ParamArr before = layer->allParams();
  ParamArr after = before;
  after[0].gamma = 2.0f;
  after[1].outWhite = 0.5f;

  auto setter = [](LevelsAdjustment* l, const ParamArr& p) {
    l->setAllParams(p);
  };
  LayerParamsCommand<LevelsAdjustment, ParamArr> cmd(layer.get(), before, after,
                                                     setter, "Levels Edit");
  cmd.apply();
  CHECK_NEAR(layer->params(LevelsChannel::Composite).gamma, 2.0f, 1e-6f);
  CHECK_NEAR(layer->params(LevelsChannel::R).outWhite, 0.5f, 1e-6f);
  cmd.undo();
  CHECK_NEAR(layer->params(LevelsChannel::Composite).gamma, 1.0f, 1e-6f);
  CHECK_NEAR(layer->params(LevelsChannel::R).outWhite, 1.0f, 1e-6f);
  cmd.redo();
  CHECK_NEAR(layer->params(LevelsChannel::Composite).gamma, 2.0f, 1e-6f);
}

int main() { return ::tuxels::testing::run(); }
