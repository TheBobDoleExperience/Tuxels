#include "layers/BrightnessContrast.h"

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

BrightnessContrast::BrightnessContrast() { rebuildLut(); }

void BrightnessContrast::setParams(const BrightnessContrastParams& p) {
  params_ = p;
  rebuildLut();
}

void BrightnessContrast::rebuildLut() {
  for (int i = 0; i < 256; ++i) {
    float v = static_cast<float>(i) / 255.f;
    v = (v - 0.5f) * (1.f + params_.contrast) + 0.5f + params_.brightness;
    lut_[i] = static_cast<uint8_t>(toByte(v));
  }
}

void BrightnessContrast::applyToAccum(TileCoord /*tc*/, Rgba32F* accum) const {
  // Shared single-LUT pass across R/G/B. Alpha passes through unchanged.
  for (int i = 0; i < kTilePixels; ++i) {
    Rgba32F& p = accum[i];
    p.r = lut_[toByte(p.r)] / 255.f;
    p.g = lut_[toByte(p.g)] / 255.f;
    p.b = lut_[toByte(p.b)] / 255.f;
  }
}

}  // namespace tuxels
