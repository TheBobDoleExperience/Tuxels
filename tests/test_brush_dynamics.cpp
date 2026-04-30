#include <cmath>
#include <cstdint>

#include "brush/BrushEngine.h"
#include "brush/RoundBrush.h"
#include "core/Pixel.h"
#include "core/TuxImage.h"
#include "test_harness.h"

namespace tuxels {

namespace {

// Compare two painted TuxImages pixel-for-pixel. Returns true only if every
// pixel matches bit-for-bit (within a tight epsilon so float-store noise
// doesn't count as drift).
bool imagesBitwiseEqual(const TuxImage& a, const TuxImage& b) {
  if (a.width() != b.width() || a.height() != b.height()) return false;
  for (int y = 0; y < a.height(); ++y) {
    for (int x = 0; x < a.width(); ++x) {
      const Rgba32F pa = a.getPixel(x, y);
      const Rgba32F pb = b.getPixel(x, y);
      if (std::fabs(pa.r - pb.r) > 1e-6f) return false;
      if (std::fabs(pa.g - pb.g) > 1e-6f) return false;
      if (std::fabs(pa.b - pb.b) > 1e-6f) return false;
      if (std::fabs(pa.a - pb.a) > 1e-6f) return false;
    }
  }
  return true;
}

RoundBrushParams basicParams() {
  RoundBrushParams p;
  p.diameter = 16;
  p.hardness = 1.f;
  p.opacity = 1.f;
  p.flow = 1.f;
  p.spacingRatio = 0.1f;
  p.color = Rgba32F(1.f, 0.f, 0.f, 1.f);
  return p;
}

void paintStroke(BrushEngine& eng, std::uint64_t seed) {
  eng.setNextStrokeSeedForTesting(seed);
  eng.beginStroke(20.f, 32.f);
  eng.continueStroke(100.f, 32.f);
  eng.endStroke();
}

}  // namespace

// --- Regression floor: jitter=0 must produce the same output as pre-S6.
// Two independently-painted strokes at jitter=0 must be byte-identical —
// the RNG path must not leak into the base render.
TEST(brush_dynamics_zero_jitter_is_deterministic) {
  RoundBrush a(basicParams());
  RoundBrush b(basicParams());
  TuxImage imgA(160, 64);
  TuxImage imgB(160, 64);
  BrushEngine engA(a, imgA);
  BrushEngine engB(b, imgB);

  engA.beginStroke(20.f, 32.f);
  engA.continueStroke(120.f, 32.f);
  engA.endStroke();

  engB.beginStroke(20.f, 32.f);
  engB.continueStroke(120.f, 32.f);
  engB.endStroke();

  CHECK(imagesBitwiseEqual(imgA, imgB));
  // Sanity: paint actually landed.
  CHECK(imgA.getPixel(50, 32).a > 0.9f);
}

// --- Known-floor check: the classic brush test's expected center alpha of
// 0.25 (opacity 0.5 * flow 0.5) is unchanged when jitter=0.
TEST(brush_dynamics_zero_jitter_preserves_opacity_math) {
  RoundBrushParams p = basicParams();
  p.diameter = 3;
  p.opacity = 0.5f;
  p.flow = 0.5f;
  p.color = Rgba32F(1.f, 1.f, 1.f, 1.f);
  RoundBrush brush(p);
  TuxImage img(5, 5);
  BrushEngine eng(brush, img);
  eng.beginStroke(2.5f, 2.5f);
  eng.endStroke();
  Rgba32F c = img.getPixel(2, 2);
  CHECK_NEAR(c.a, 0.25f, 1e-4);
  CHECK_NEAR(c.r, 0.25f, 1e-4);
}

// --- Jittered strokes must be reproducible given the same pinned seed.
TEST(brush_dynamics_same_seed_matches_bitwise) {
  RoundBrushParams p = basicParams();
  p.sizeJitter = 0.5f;
  p.opacityJitter = 0.5f;
  RoundBrush a(p);
  RoundBrush b(p);
  TuxImage imgA(160, 64);
  TuxImage imgB(160, 64);
  BrushEngine engA(a, imgA);
  BrushEngine engB(b, imgB);

  paintStroke(engA, 42);
  paintStroke(engB, 42);

  CHECK(imagesBitwiseEqual(imgA, imgB));
  // Jittered output must still deposit paint along the stroke.
  CHECK(imgA.getPixel(60, 32).a > 0.1f);
}

// --- Different seeds must produce different pixel output (else the RNG
// isn't actually being consumed). Checks jitter is wired, not just clamped
// away.
TEST(brush_dynamics_different_seeds_diverge) {
  RoundBrushParams p = basicParams();
  p.sizeJitter = 0.5f;
  p.opacityJitter = 0.5f;
  RoundBrush a(p);
  RoundBrush b(p);
  TuxImage imgA(160, 64);
  TuxImage imgB(160, 64);
  BrushEngine engA(a, imgA);
  BrushEngine engB(b, imgB);

  paintStroke(engA, 42);
  paintStroke(engB, 1337);

  CHECK(!imagesBitwiseEqual(imgA, imgB));
}

// --- Opacity jitter at half strength on opaque color + base opacity 1.0
// must produce alpha values within [0, 1] — can't blow past 1.
TEST(brush_dynamics_opacity_jitter_stays_bounded) {
  RoundBrushParams p = basicParams();
  p.diameter = 12;
  p.opacityJitter = 0.75f;
  RoundBrush brush(p);
  TuxImage img(80, 32);
  BrushEngine eng(brush, img);
  eng.setNextStrokeSeedForTesting(7);
  eng.beginStroke(20.f, 16.f);
  eng.continueStroke(60.f, 16.f);
  eng.endStroke();

  for (int y = 0; y < img.height(); ++y) {
    for (int x = 0; x < img.width(); ++x) {
      const Rgba32F c = img.getPixel(x, y);
      CHECK(c.a >= 0.f);
      CHECK(c.a <= 1.f + 1e-5f);
    }
  }
}

// --- Size jitter must change effective stamp radius per stamp. At a high
// jitter value with a seed that forces at least one shrinking stamp,
// a long stroke still paints center pixels but shows alpha variance — a
// non-jittered stroke with identical params produces a uniform band.
TEST(brush_dynamics_size_jitter_varies_row_profile) {
  RoundBrushParams baseP = basicParams();
  baseP.diameter = 10;
  baseP.hardness = 1.f;
  baseP.opacity = 1.f;
  baseP.flow = 1.f;
  baseP.spacingRatio = 0.1f;

  // Baseline: no jitter → uniform band along y=center.
  RoundBrush baseBrush(baseP);
  TuxImage baseImg(200, 32);
  BrushEngine baseEng(baseBrush, baseImg);
  baseEng.beginStroke(20.f, 16.f);
  baseEng.continueStroke(180.f, 16.f);
  baseEng.endStroke();

  // Jittered: same params + high size jitter → band should still be
  // mostly opaque at center, but alpha at a fixed off-center row varies
  // vs the baseline because effective diameter changes per stamp.
  RoundBrushParams jp = baseP;
  jp.sizeJitter = 0.9f;
  RoundBrush jitBrush(jp);
  TuxImage jitImg(200, 32);
  BrushEngine jitEng(jitBrush, jitImg);
  jitEng.setNextStrokeSeedForTesting(99);
  jitEng.beginStroke(20.f, 16.f);
  jitEng.continueStroke(180.f, 16.f);
  jitEng.endStroke();

  // Both strokes must cover the stroke center.
  CHECK(baseImg.getPixel(100, 16).a > 0.9f);
  CHECK(jitImg.getPixel(100, 16).a > 0.5f);

  // Jittered stroke must differ from baseline somewhere along the path —
  // they were identical inputs apart from sizeJitter.
  bool anyDifference = false;
  for (int x = 20; x < 180 && !anyDifference; ++x) {
    for (int y = 0; y < 32; ++y) {
      const float da = std::fabs(baseImg.getPixel(x, y).a -
                                 jitImg.getPixel(x, y).a);
      if (da > 1e-3f) {
        anyDifference = true;
        break;
      }
    }
  }
  CHECK(anyDifference);
}

// --- Spacing slider changes stamp count: tighter spacing → more stamps
// → denser coverage at the stroke edges.
TEST(brush_dynamics_spacing_ratio_affects_density) {
  RoundBrushParams p;
  p.diameter = 10;
  p.hardness = 1.f;
  p.opacity = 0.2f;  // low enough that individual stamps leave a trace
  p.flow = 1.f;
  p.color = Rgba32F(1.f, 1.f, 1.f, 1.f);

  // Dense spacing → many stamps → center builds up higher alpha.
  p.spacingRatio = 0.05f;
  RoundBrush dense(p);
  TuxImage denseImg(200, 32);
  BrushEngine denseEng(dense, denseImg);
  denseEng.beginStroke(20.f, 16.f);
  denseEng.continueStroke(180.f, 16.f);
  denseEng.endStroke();

  // Sparse spacing → fewer stamps → lower built-up alpha along the path.
  p.spacingRatio = 1.0f;  // stamp at every 10 px
  RoundBrush sparse(p);
  TuxImage sparseImg(200, 32);
  BrushEngine sparseEng(sparse, sparseImg);
  sparseEng.beginStroke(20.f, 16.f);
  sparseEng.continueStroke(180.f, 16.f);
  sparseEng.endStroke();

  // Measure average alpha along the stroke row for both.
  float denseSum = 0.f, sparseSum = 0.f;
  int count = 0;
  for (int x = 30; x < 170; ++x) {
    denseSum += denseImg.getPixel(x, 16).a;
    sparseSum += sparseImg.getPixel(x, 16).a;
    ++count;
  }
  const float denseAvg = denseSum / count;
  const float sparseAvg = sparseSum / count;
  CHECK(denseAvg > sparseAvg);
}

// ---------- M8-S1: stylus pressure scaling ----------

TEST(brush_pressure_full_is_bitwise_identical_to_pre_m8) {
  // Default pressure (1.0) on a no-jitter brush must reproduce the exact
  // pre-M8 path. Sampling the engine output here pins the regression
  // protection against future refactors that disturb the no-jitter base
  // case.
  RoundBrushParams p = basicParams();
  RoundBrush b(p);
  TuxImage a(48, 48), c(48, 48);

  BrushEngine ea(b, a);
  ea.setPressure(1.0f);
  ea.beginStroke(24.f, 24.f);
  ea.endStroke();

  BrushEngine ec(b, c);
  // No setPressure call → pressure stays at default 1.0 — the engines'
  // outputs must match.
  ec.beginStroke(24.f, 24.f);
  ec.endStroke();

  CHECK(imagesBitwiseEqual(a, c));
}

TEST(brush_pressure_zero_lays_minimal_or_no_pixels) {
  RoundBrushParams p = basicParams();
  RoundBrush b(p);
  TuxImage img(48, 48);

  BrushEngine eng(b, img);
  eng.setPressure(0.0f);
  eng.beginStroke(24.f, 24.f);
  eng.endStroke();

  // opEff = 1 * 0 = 0 → every stamp pixel writes a == 0 → nothing
  // visible. The exact tile bookkeeping may still allocate a tile (the
  // stamp loop touches setPixel even with a == 0; sometimes it short-
  // circuits via `a <= 0.f → continue`). Either way, no opaque ink.
  for (int y = 0; y < img.height(); ++y) {
    for (int x = 0; x < img.width(); ++x) {
      const Rgba32F pix = img.getPixel(x, y);
      CHECK(pix.a < 1e-6f);
    }
  }
}

TEST(brush_pressure_half_paints_lighter_than_full) {
  // Pressure 0.5 → smaller diameter + halved opacity. The total ink
  // coverage (sum of alpha) is strictly less than at pressure 1.0.
  auto inkSum = [](const TuxImage& img) {
    double sum = 0.0;
    for (int y = 0; y < img.height(); ++y) {
      for (int x = 0; x < img.width(); ++x) {
        sum += img.getPixel(x, y).a;
      }
    }
    return sum;
  };

  RoundBrushParams p = basicParams();
  RoundBrush b(p);
  TuxImage hi(48, 48), lo(48, 48);

  BrushEngine eHi(b, hi);
  eHi.setPressure(1.0f);
  eHi.beginStroke(24.f, 24.f);
  eHi.endStroke();

  BrushEngine eLo(b, lo);
  eLo.setPressure(0.5f);
  eLo.beginStroke(24.f, 24.f);
  eLo.endStroke();

  CHECK(inkSum(hi) > inkSum(lo));
  CHECK(inkSum(lo) > 0.0);
}

}  // namespace tuxels

int main() { return tuxels::testing::run(); }
