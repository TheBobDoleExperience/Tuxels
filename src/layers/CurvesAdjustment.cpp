#include "layers/CurvesAdjustment.h"

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

CurvesAdjustment::CurvesAdjustment() {
  for (int c = 0; c < 4; ++c) {
    points_[c] = {{0.f, 0.f}, {1.f, 1.f}};
    rebuildLut(c);
  }
}

void CurvesAdjustment::setPoints(CurvesChannel ch,
                                 std::vector<SplinePoint> pts) {
  const int c = static_cast<int>(ch);
  points_[c] = std::move(pts);
  rebuildLut(c);
}

const std::vector<SplinePoint>& CurvesAdjustment::points(
    CurvesChannel ch) const {
  return points_[static_cast<int>(ch)];
}

void CurvesAdjustment::setAllPoints(const PointsArray& pts) {
  points_ = pts;
  for (int c = 0; c < 4; ++c) rebuildLut(c);
}

void CurvesAdjustment::rebuildLut(int ch) {
  buildLut256(points_[ch], lut_[ch]);
}

void CurvesAdjustment::applyToAccum(TileCoord /*tc*/, Rgba32F* accum) const {
  const uint8_t* lc = lut_[0];  // composite
  const uint8_t* lr = lut_[1];
  const uint8_t* lg = lut_[2];
  const uint8_t* lb = lut_[3];
  for (int i = 0; i < kTilePixels; ++i) {
    Rgba32F& p = accum[i];
    const int ri = lc[toByte(p.r)];
    const int gi = lc[toByte(p.g)];
    const int bi = lc[toByte(p.b)];
    p.r = lr[ri] / 255.f;
    p.g = lg[gi] / 255.f;
    p.b = lb[bi] / 255.f;
    // Alpha untouched — curves never modifies transparency.
  }
}

}  // namespace tuxels
