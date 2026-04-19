#include <cmath>

#include "compositor/blend.h"
#include "test_harness.h"

using namespace tuxels;

static constexpr float kEps = 1e-5f;

// Per-channel sanity: Normal is pass-through.
TEST(normal_is_passthrough) {
  CHECK_NEAR(applyBlend(BlendMode::Normal, 0.0f, 0.0f), 0.0f, kEps);
  CHECK_NEAR(applyBlend(BlendMode::Normal, 0.5f, 0.25f), 0.25f, kEps);
  CHECK_NEAR(applyBlend(BlendMode::Normal, 1.0f, 0.75f), 0.75f, kEps);
}

TEST(multiply_spot_checks) {
  CHECK_NEAR(applyBlend(BlendMode::Multiply, 0.0f, 0.5f), 0.0f, kEps);
  CHECK_NEAR(applyBlend(BlendMode::Multiply, 1.0f, 0.5f), 0.5f, kEps);
  CHECK_NEAR(applyBlend(BlendMode::Multiply, 0.5f, 0.5f), 0.25f, kEps);
}

TEST(screen_spot_checks) {
  CHECK_NEAR(applyBlend(BlendMode::Screen, 0.0f, 0.5f), 0.5f, kEps);
  CHECK_NEAR(applyBlend(BlendMode::Screen, 1.0f, 0.5f), 1.0f, kEps);
  CHECK_NEAR(applyBlend(BlendMode::Screen, 0.5f, 0.5f), 0.75f, kEps);
}

TEST(darken_returns_minimum) {
  CHECK_NEAR(applyBlend(BlendMode::Darken, 0.2f, 0.8f), 0.2f, kEps);
  CHECK_NEAR(applyBlend(BlendMode::Darken, 0.8f, 0.2f), 0.2f, kEps);
  CHECK_NEAR(applyBlend(BlendMode::Darken, 0.5f, 0.5f), 0.5f, kEps);
}

TEST(lighten_returns_maximum) {
  CHECK_NEAR(applyBlend(BlendMode::Lighten, 0.2f, 0.8f), 0.8f, kEps);
  CHECK_NEAR(applyBlend(BlendMode::Lighten, 0.8f, 0.2f), 0.8f, kEps);
  CHECK_NEAR(applyBlend(BlendMode::Lighten, 0.5f, 0.5f), 0.5f, kEps);
}

TEST(difference_is_absolute_delta) {
  CHECK_NEAR(applyBlend(BlendMode::Difference, 0.2f, 0.8f), 0.6f, kEps);
  CHECK_NEAR(applyBlend(BlendMode::Difference, 0.8f, 0.2f), 0.6f, kEps);
  CHECK_NEAR(applyBlend(BlendMode::Difference, 0.5f, 0.5f), 0.0f, kEps);
}

TEST(exclusion_formula) {
  // cb + cs - 2*cb*cs
  CHECK_NEAR(applyBlend(BlendMode::Exclusion, 0.0f, 0.0f), 0.0f, kEps);
  CHECK_NEAR(applyBlend(BlendMode::Exclusion, 1.0f, 1.0f), 0.0f, kEps);
  CHECK_NEAR(applyBlend(BlendMode::Exclusion, 0.5f, 0.5f), 0.5f, kEps);
}

TEST(color_burn_edge_cases) {
  CHECK_NEAR(applyBlend(BlendMode::ColorBurn, 0.5f, 0.0f), 0.0f, kEps);
  CHECK_NEAR(applyBlend(BlendMode::ColorBurn, 1.0f, 0.0f), 1.0f, kEps);  // white backdrop stays
  CHECK_NEAR(applyBlend(BlendMode::ColorBurn, 0.5f, 1.0f), 0.5f, kEps);  // source=1 no burn
  // cb=0.8, cs=0.5 → 1 - min(1, (1-0.8)/0.5) = 1 - 0.4 = 0.6
  CHECK_NEAR(applyBlend(BlendMode::ColorBurn, 0.8f, 0.5f), 0.6f, kEps);
}

TEST(color_dodge_edge_cases) {
  CHECK_NEAR(applyBlend(BlendMode::ColorDodge, 0.0f, 0.5f), 0.0f, kEps);
  CHECK_NEAR(applyBlend(BlendMode::ColorDodge, 0.5f, 1.0f), 1.0f, kEps);
  CHECK_NEAR(applyBlend(BlendMode::ColorDodge, 0.5f, 0.0f), 0.5f, kEps);  // cs=0 no dodge
  // cb=0.3, cs=0.5 → min(1, 0.3/0.5) = 0.6
  CHECK_NEAR(applyBlend(BlendMode::ColorDodge, 0.3f, 0.5f), 0.6f, kEps);
}

TEST(overlay_branches) {
  // cb<=0.5 branch: 2*cb*cs
  CHECK_NEAR(applyBlend(BlendMode::Overlay, 0.25f, 0.5f), 0.25f, kEps);
  // cb>0.5 branch: 1 - 2*(1-cb)*(1-cs)
  CHECK_NEAR(applyBlend(BlendMode::Overlay, 0.75f, 0.5f), 0.75f, kEps);
  CHECK_NEAR(applyBlend(BlendMode::Overlay, 0.5f, 0.5f), 0.5f, kEps);
}

TEST(hard_light_branches_on_source) {
  // cs<=0.5 branch: 2*cb*cs
  CHECK_NEAR(applyBlend(BlendMode::HardLight, 0.5f, 0.25f), 0.25f, kEps);
  // cs>0.5 branch: 1 - 2*(1-cb)*(1-cs)
  CHECK_NEAR(applyBlend(BlendMode::HardLight, 0.5f, 0.75f), 0.75f, kEps);
}

TEST(soft_light_spot_checks) {
  // At cs=0.5 the (1 - 2cs) term is zero, so cb passes through.
  CHECK_NEAR(applyBlend(BlendMode::SoftLight, 0.5f, 0.5f), 0.5f, kEps);
  CHECK_NEAR(applyBlend(BlendMode::SoftLight, 0.8f, 0.5f), 0.8f, kEps);
  // cs=0: cb - 1 * cb * (1 - cb) = cb * cb  →  0.5² = 0.25
  CHECK_NEAR(applyBlend(BlendMode::SoftLight, 0.5f, 0.0f), 0.25f, kEps);
  // cs=1, cb=0.25 (in the lower-D piece): d = ((16·0.25 − 12)·0.25 + 4)·0.25 = 0.5
  // result = cb + (2·1 − 1)·(d − cb) = 0.25 + 0.25 = 0.5
  CHECK_NEAR(applyBlend(BlendMode::SoftLight, 0.25f, 1.0f), 0.5f, kEps);
  // cs=1, cb=0.64 (upper piece, d = sqrt(cb) = 0.8)
  // result = 0.64 + 1·(0.8 − 0.64) = 0.8
  CHECK_NEAR(applyBlend(BlendMode::SoftLight, 0.64f, 1.0f), 0.8f, kEps);
}

// Composite-level checks: full Rgba32F composite with straight-alpha math.

TEST(composite_opaque_backdrop_normal) {
  Rgba32F bd(1.f, 0.f, 0.f, 1.f);
  Rgba32F src(0.f, 1.f, 0.f, 0.5f);
  Rgba32F r = compositePixel(bd, src, BlendMode::Normal, 1.f, 0u, 0, 0);
  // 50% lerp: red→half green
  CHECK_NEAR(r.r, 0.5f, kEps);
  CHECK_NEAR(r.g, 0.5f, kEps);
  CHECK_NEAR(r.b, 0.0f, kEps);
  CHECK_NEAR(r.a, 1.0f, kEps);
}

TEST(composite_transparent_backdrop_yields_source) {
  Rgba32F bd = Rgba32F::transparent();
  Rgba32F src(0.25f, 0.5f, 0.75f, 1.f);
  Rgba32F r = compositePixel(bd, src, BlendMode::Multiply, 1.f, 0u, 0, 0);
  // No backdrop means blend is just source.
  CHECK_NEAR(r.r, 0.25f, kEps);
  CHECK_NEAR(r.g, 0.5f, kEps);
  CHECK_NEAR(r.b, 0.75f, kEps);
  CHECK_NEAR(r.a, 1.0f, kEps);
}

TEST(composite_multiply_opaque_pair) {
  Rgba32F bd(0.5f, 0.5f, 0.5f, 1.f);
  Rgba32F src(0.5f, 0.5f, 0.5f, 1.f);
  Rgba32F r = compositePixel(bd, src, BlendMode::Multiply, 1.f, 0u, 0, 0);
  CHECK_NEAR(r.r, 0.25f, kEps);
  CHECK_NEAR(r.a, 1.0f, kEps);
}

TEST(composite_layer_opacity_scales_src_alpha) {
  Rgba32F bd(1.f, 0.f, 0.f, 1.f);
  Rgba32F src(0.f, 1.f, 0.f, 1.f);
  // opacity=0 → pure backdrop
  Rgba32F r0 = compositePixel(bd, src, BlendMode::Normal, 0.f, 0u, 0, 0);
  CHECK_NEAR(r0.r, 1.0f, kEps);
  CHECK_NEAR(r0.g, 0.0f, kEps);
  // opacity=1 → pure source
  Rgba32F r1 = compositePixel(bd, src, BlendMode::Normal, 1.f, 0u, 0, 0);
  CHECK_NEAR(r1.r, 0.0f, kEps);
  CHECK_NEAR(r1.g, 1.0f, kEps);
}

TEST(composite_dissolve_deterministic_and_bounded) {
  // With as=0 no pixel ever lands; with as=1 every pixel lands.
  Rgba32F bd(0.5f, 0.5f, 0.5f, 1.f);
  Rgba32F src(1.f, 0.f, 0.f, 0.f);
  Rgba32F r0 = compositePixel(bd, src, BlendMode::Dissolve, 1.f, 1234u, 7, 11);
  CHECK_NEAR(r0.r, 0.5f, kEps);  // unchanged

  Rgba32F src_full(1.f, 0.f, 0.f, 1.f);
  Rgba32F r1 = compositePixel(bd, src_full, BlendMode::Dissolve, 1.f, 1234u, 7, 11);
  CHECK_NEAR(r1.r, 1.0f, kEps);
  CHECK_NEAR(r1.g, 0.0f, kEps);
  CHECK_NEAR(r1.a, 1.0f, kEps);

  // Determinism: same inputs, same output.
  Rgba32F r1b = compositePixel(bd, src_full, BlendMode::Dissolve, 1.f, 1234u, 7, 11);
  CHECK_NEAR(r1.r, r1b.r, kEps);
  CHECK_NEAR(r1.g, r1b.g, kEps);
}

int main() { return tuxels::testing::run(); }
