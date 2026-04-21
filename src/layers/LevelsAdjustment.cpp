#include "layers/LevelsAdjustment.h"

#include <algorithm>
#include <cmath>

#include "core/Tile.h"

namespace tuxels {

namespace {

inline int toByte(float v) {
  v = std::clamp(v, 0.f, 1.f);
  return static_cast<int>(std::lround(v * 255.f));
}

}  // namespace

LevelsAdjustment::LevelsAdjustment() {
  for (int c = 0; c < 4; ++c) rebuildLut(c);
}

void LevelsAdjustment::setParams(LevelsChannel ch, const LevelsParams& p) {
  const int c = static_cast<int>(ch);
  params_[c] = p;
  rebuildLut(c);
}

const LevelsParams& LevelsAdjustment::params(LevelsChannel ch) const {
  return params_[static_cast<int>(ch)];
}

void LevelsAdjustment::setAllParams(const std::array<LevelsParams, 4>& p) {
  params_ = p;
  for (int c = 0; c < 4; ++c) rebuildLut(c);
}

void LevelsAdjustment::rebuildLut(int ch) {
  const LevelsParams& p = params_[ch];
  // Degenerate range (inWhite <= inBlack) would divide by ~0 below and
  // produce NaN/Inf; clamp the denominator to a small positive value so the
  // LUT stays well-defined. Matches PS behaviour: the slider UI prevents
  // crossing, but a loaded `.txl` could in theory carry pathological values.
  const float denom = std::max(1e-6f, p.inWhite - p.inBlack);
  const float invGamma = 1.f / std::max(1e-6f, p.gamma);
  const float outRange = p.outWhite - p.outBlack;
  for (int i = 0; i < 256; ++i) {
    float v = static_cast<float>(i) / 255.f;
    v = std::clamp((v - p.inBlack) / denom, 0.f, 1.f);
    v = std::pow(v, invGamma);
    v = p.outBlack + v * outRange;
    lut_[ch][i] = static_cast<uint8_t>(toByte(v));
  }
}

void LevelsAdjustment::applyToAccum(TileCoord /*tc*/, Rgba32F* accum) const {
  const uint8_t* lc = lut_[0];  // composite
  const uint8_t* lr = lut_[1];
  const uint8_t* lg = lut_[2];
  const uint8_t* lb = lut_[3];
  for (int i = 0; i < kTilePixels; ++i) {
    Rgba32F& p = accum[i];
    // Composite first (acts on every channel identically), then per-channel.
    const int ri = lc[toByte(p.r)];
    const int gi = lc[toByte(p.g)];
    const int bi = lc[toByte(p.b)];
    p.r = lr[ri] / 255.f;
    p.g = lg[gi] / 255.f;
    p.b = lb[bi] / 255.f;
    // Alpha passes through unchanged — Levels never touches transparency.
  }
}

}  // namespace tuxels
