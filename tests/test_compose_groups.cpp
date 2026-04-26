#include <cstring>
#include <memory>

#include "compositor/compose.h"
#include "core/Pixel.h"
#include "core/TuxImage.h"
#include "layers/AdjustmentLayer.h"
#include "layers/GroupLayer.h"
#include "layers/LayerMask.h"
#include "layers/LayerTree.h"
#include "layers/PixelLayer.h"
#include "test_harness.h"

using namespace tuxels;

namespace {

constexpr Rgba32F kRed{1.f, 0.f, 0.f, 1.f};
constexpr Rgba32F kGreen{0.f, 1.f, 0.f, 1.f};
constexpr Rgba32F kBlue{0.f, 0.f, 1.f, 1.f};
constexpr Rgba32F kCyan{0.f, 1.f, 1.f, 1.f};

// Test-only adjustment that negates RGB. Same as test_adjustment_layer.cpp.
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

std::unique_ptr<InvertAdjustment> invertLayer(LayerId id) {
  auto a = std::make_unique<InvertAdjustment>();
  a->id = id;
  a->opacity = 1.f;
  // Invert is a true adjustment in tests above without a mask; compose
  // gracefully handles a null mask, so we follow that pattern here.
  return a;
}

// Construct a doc-sized white mask gated on the right half (x >= W/2),
// black on the left. Returns a LayerMask owned by the caller.
std::unique_ptr<LayerMask> rightHalfMask(int w, int h) {
  auto m = std::make_unique<LayerMask>(w, h);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      m->image.setPixel(x, y,
                        x < w / 2 ? Rgba32F{0.f, 0.f, 0.f, 1.f}
                                  : Rgba32F{1.f, 1.f, 1.f, 1.f});
    }
  }
  m->enabled = true;
  return m;
}

bool tilesEqual(const TuxImage& a, const TuxImage& b, float eps = 1e-5f) {
  if (a.width() != b.width() || a.height() != b.height()) return false;
  for (int y = 0; y < a.height(); ++y) {
    for (int x = 0; x < a.width(); ++x) {
      if (!approxEqual(a.getPixel(x, y), b.getPixel(x, y), eps)) return false;
    }
  }
  return true;
}

}  // namespace

TEST(compose_pass_through_group_equals_flat) {
  // Flat: [red bg, green fg]
  LayerTree flat;
  flat.add(solidPixelLayer(32, 32, kRed, 1));
  flat.add(solidPixelLayer(32, 32, kGreen, 2));

  // Grouped: [red bg, group{Pass-Through, [green fg]}]
  LayerTree grouped;
  grouped.add(solidPixelLayer(32, 32, kRed, 11));
  auto g = std::make_unique<GroupLayer>();
  g->id = 12;
  // Default blend is PassThrough already; explicit for clarity.
  g->blend = BlendMode::PassThrough;
  g->children.push_back(solidPixelLayer(32, 32, kGreen, 13));
  grouped.add(std::move(g));

  TuxImage flatOut(32, 32), grpOut(32, 32);
  compose(flat, flatOut);
  compose(grouped, grpOut);
  CHECK(tilesEqual(flatOut, grpOut));
  // Sanity: green wins at the top.
  CHECK(approxEqual(grpOut.getPixel(8, 8), kGreen));
}

TEST(compose_pass_through_adjustment_leaks_out) {
  // Pass-Through means an adjustment inside a group still applies to
  // layers below the group (the group is purely organizational).
  // Setup: [red bg, group{PassThrough, [invert]}] should equal
  // [red bg, invert] = cyan everywhere.
  LayerTree tree;
  tree.add(solidPixelLayer(32, 32, kRed, 1));
  auto g = std::make_unique<GroupLayer>();
  g->id = 2;
  g->blend = BlendMode::PassThrough;
  g->children.push_back(invertLayer(3));
  tree.add(std::move(g));

  TuxImage out(32, 32);
  compose(tree, out);
  CHECK(approxEqual(out.getPixel(8, 8), kCyan));
}

TEST(compose_isolated_group_with_adjustment_doesnt_leak) {
  // Isolated group (Normal blend) means children composite into a private
  // buffer first. The invert inside only sees the group's own contents
  // (initially transparent), not the bg below.
  // Setup: [red bg, group{Normal, [invert]}]
  // Invert on a transparent accumulator: alpha stays 0, so the back-
  // composite contributes nothing. Result: bg unchanged = red.
  LayerTree tree;
  tree.add(solidPixelLayer(32, 32, kRed, 1));
  auto g = std::make_unique<GroupLayer>();
  g->id = 2;
  g->blend = BlendMode::Normal;
  g->children.push_back(invertLayer(3));
  tree.add(std::move(g));

  TuxImage out(32, 32);
  compose(tree, out);
  CHECK(approxEqual(out.getPixel(8, 8), kRed));
}

TEST(compose_isolated_group_opacity_multiplies) {
  // [red bg, group{Normal, opacity=0.5, [green fg]}]
  // Group's accum2 is solid green; back-composite at opacity 0.5 gives
  // a 50/50 mix red↔green.
  LayerTree tree;
  tree.add(solidPixelLayer(32, 32, kRed, 1));
  auto g = std::make_unique<GroupLayer>();
  g->id = 2;
  g->blend = BlendMode::Normal;
  g->opacity = 0.5f;
  g->children.push_back(solidPixelLayer(32, 32, kGreen, 3));
  tree.add(std::move(g));

  TuxImage out(32, 32);
  compose(tree, out);
  const Rgba32F p = out.getPixel(8, 8);
  CHECK_NEAR(p.r, 0.5f, 1e-5f);
  CHECK_NEAR(p.g, 0.5f, 1e-5f);
  CHECK_NEAR(p.b, 0.f, 1e-5f);
  CHECK_NEAR(p.a, 1.f, 1e-5f);
}

TEST(compose_group_mask_gates_output) {
  // [red bg, group{Normal, mask=right-half, [green fg]}]
  // Left half: mask=0 → group contributes 0 → bg only (red).
  // Right half: mask=1 → group fully opaque green.
  LayerTree tree;
  tree.add(solidPixelLayer(32, 32, kRed, 1));
  auto g = std::make_unique<GroupLayer>();
  g->id = 2;
  g->blend = BlendMode::Normal;
  g->mask = rightHalfMask(32, 32);
  g->children.push_back(solidPixelLayer(32, 32, kGreen, 3));
  tree.add(std::move(g));

  TuxImage out(32, 32);
  compose(tree, out);
  CHECK(approxEqual(out.getPixel(4, 16), kRed));
  CHECK(approxEqual(out.getPixel(20, 16), kGreen));
}

TEST(compose_nested_pass_through_inside_isolated) {
  // [red bg, group{Normal, [group{PassThrough, [invert]}]}]
  // Outer is isolated, so the inner Pass-Through sees only the group's
  // private accum (transparent) — invert on transparent stays transparent.
  // Back-composite contributes nothing. Result: red bg unchanged.
  LayerTree tree;
  tree.add(solidPixelLayer(32, 32, kRed, 1));
  auto outer = std::make_unique<GroupLayer>();
  outer->id = 2;
  outer->blend = BlendMode::Normal;
  auto inner = std::make_unique<GroupLayer>();
  inner->id = 3;
  inner->blend = BlendMode::PassThrough;
  inner->children.push_back(invertLayer(4));
  outer->children.push_back(std::move(inner));
  tree.add(std::move(outer));

  TuxImage out(32, 32);
  compose(tree, out);
  CHECK(approxEqual(out.getPixel(8, 8), kRed));
}

TEST(compose_clipped_group_no_base_below_is_noop) {
  // [group{Normal, clipToBelow=true, [green]}, red bg]  — wait, group is
  // first so there's no base below. The group should be a no-op (skip),
  // and red bg composites on top normally.
  // Actually order: bottom-up. Group is at index 0 (bottom) with no base
  // below; red is on top. Compose iterates bottom→top.
  LayerTree tree;
  auto g = std::make_unique<GroupLayer>();
  g->id = 1;
  g->blend = BlendMode::Normal;
  g->clipToBelow = true;
  g->children.push_back(solidPixelLayer(32, 32, kGreen, 2));
  tree.add(std::move(g));
  tree.add(solidPixelLayer(32, 32, kRed, 3));

  TuxImage out(32, 32);
  compose(tree, out);
  // No green should appear — group is no-op; only red on top of empty.
  CHECK(approxEqual(out.getPixel(8, 8), kRed));
}

TEST(compose_clipped_group_gates_by_base_alpha) {
  // [bg=red, half-alpha-blue, group{Normal, clipToBelow=true, [green]}]
  // The half-alpha-blue layer has alpha 0.5 → lastBaseAlpha = 0.5.
  // The clipped group multiplies its back-composite factor by 0.5, so
  // the green contribution is at half strength.
  LayerTree tree;
  tree.add(solidPixelLayer(32, 32, kRed, 1));
  auto base = std::make_unique<PixelLayer>(32, 32);
  base->id = 2;
  base->image.fill(Rgba32F{0.f, 0.f, 1.f, 0.5f});  // half-alpha blue
  tree.add(std::move(base));
  auto g = std::make_unique<GroupLayer>();
  g->id = 3;
  g->blend = BlendMode::Normal;
  g->clipToBelow = true;
  g->children.push_back(solidPixelLayer(32, 32, kGreen, 4));
  tree.add(std::move(g));

  TuxImage out(32, 32);
  compose(tree, out);
  // The first two layers compose to: red blended with half-alpha blue =
  // (0.5*1 + 0.5*0, 0, 0.5*1) = (0.5, 0, 0.5) at full alpha.
  // The clipped group contributes green at f = 0.5 (gated by base alpha).
  // Lerp: result = post-base * 0.5 + green * 0.5
  //              = (0.5*0.5, 0.5*1+0.5*0, 0.5*0.5) = ... actually compositePixel
  // does Normal blending with effective opacity 0.5 over (0.5, 0, 0.5).
  // Normal: out = src*a + dst*(1-a) = (0,1,0)*0.5 + (0.5,0,0.5)*0.5
  //             = (0.25, 0.5, 0.25)
  const Rgba32F p = out.getPixel(8, 8);
  CHECK_NEAR(p.r, 0.25f, 1e-5f);
  CHECK_NEAR(p.g, 0.5f, 1e-5f);
  CHECK_NEAR(p.b, 0.25f, 1e-5f);
  CHECK_NEAR(p.a, 1.f, 1e-5f);
}

TEST(compose_clipped_adjustment_inside_isolated_gates_group_local) {
  // Inside an isolated group: [pixelA half-and-half, clipped invert].
  // The clipped invert inside the group should gate against pixelA's
  // alpha (group-local), not against any pixel layer outside.
  // Setup: [bg=blue (outside), group{Normal, [right-half-red, clipped-invert]}]
  // The bg=blue is OUTSIDE the group; the clipped invert inside should
  // only affect where right-half-red has alpha > 0 (right half), regardless
  // of the bg's alpha. After back-composite (group is opaque red on right,
  // transparent on left), final = blue (left) + inverted-red=cyan (right).
  LayerTree tree;
  tree.add(solidPixelLayer(32, 32, kBlue, 1));  // outside
  auto g = std::make_unique<GroupLayer>();
  g->id = 2;
  g->blend = BlendMode::Normal;
  // pixelA: red on right half, transparent on left.
  auto right = std::make_unique<PixelLayer>(32, 32);
  right->id = 3;
  for (int y = 0; y < 32; ++y) {
    for (int x = 0; x < 32; ++x) {
      right->image.setPixel(x, y, x < 16 ? Rgba32F{0.f, 0.f, 0.f, 0.f}
                                          : kRed);
    }
  }
  g->children.push_back(std::move(right));
  // Clipped invert above pixelA (inside the group).
  auto inv = invertLayer(4);
  inv->clipToBelow = true;
  g->children.push_back(std::move(inv));
  tree.add(std::move(g));

  TuxImage out(32, 32);
  compose(tree, out);
  // Left half: bg blue (no group contribution since pixelA is transparent
  // there → group's accum is transparent → no back-composite).
  CHECK(approxEqual(out.getPixel(4, 16), kBlue));
  // Right half: pixelA = red, then clipped invert flips it to cyan within
  // the group; back-composite places cyan over blue (cyan is opaque).
  CHECK(approxEqual(out.getPixel(20, 16), kCyan));
}

TEST(compose_empty_group_is_noop) {
  LayerTree tree;
  tree.add(solidPixelLayer(32, 32, kRed, 1));
  auto g = std::make_unique<GroupLayer>();
  g->id = 2;
  g->blend = BlendMode::Normal;
  // No children.
  tree.add(std::move(g));
  tree.add(solidPixelLayer(32, 32, kGreen, 3));

  TuxImage out(32, 32);
  compose(tree, out);
  // Empty group contributes nothing; result is green over red = green.
  CHECK(approxEqual(out.getPixel(8, 8), kGreen));
}

TEST(compose_invisible_group_skips_recursion) {
  // [bg=red, group{visible=false, [green]}] — the group's children must not
  // contribute. The visibility check is at the group level, not per-child.
  LayerTree tree;
  tree.add(solidPixelLayer(32, 32, kRed, 1));
  auto g = std::make_unique<GroupLayer>();
  g->id = 2;
  g->visible = false;
  g->blend = BlendMode::Normal;
  g->children.push_back(solidPixelLayer(32, 32, kGreen, 3));
  tree.add(std::move(g));

  TuxImage out(32, 32);
  compose(tree, out);
  CHECK(approxEqual(out.getPixel(8, 8), kRed));
}

TEST(compose_pass_through_with_half_opacity_lerps) {
  // [bg=red, group{PassThrough, opacity=0.5, [invert]}]
  // PassThrough at opacity 0.5: invert leaks out, but the lerp-back-to-
  // pre-recursion at f=0.5 mixes pre (red) with post (cyan) 50/50.
  // Expected: ((1+0)/2, (0+1)/2, (0+1)/2) = (0.5, 0.5, 0.5).
  LayerTree tree;
  tree.add(solidPixelLayer(32, 32, kRed, 1));
  auto g = std::make_unique<GroupLayer>();
  g->id = 2;
  g->blend = BlendMode::PassThrough;
  g->opacity = 0.5f;
  g->children.push_back(invertLayer(3));
  tree.add(std::move(g));

  TuxImage out(32, 32);
  compose(tree, out);
  const Rgba32F p = out.getPixel(8, 8);
  CHECK_NEAR(p.r, 0.5f, 1e-5f);
  CHECK_NEAR(p.g, 0.5f, 1e-5f);
  CHECK_NEAR(p.b, 0.5f, 1e-5f);
}

int main() { return ::tuxels::testing::run(); }
