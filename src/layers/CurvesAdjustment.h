#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "geom/Spline.h"
#include "layers/AdjustmentLayer.h"

namespace tuxels {

// Channel selector for Curves. Identical shape to LevelsChannel but kept
// separate so the enum values can't accidentally be passed to the wrong
// layer type.
enum class CurvesChannel : int {
  Composite = 0,
  R = 1,
  G = 2,
  B = 3,
};

// Non-destructive per-channel curves adjustment. Each channel owns an
// ordered list of control points in `[0, 1]²`; the cached LUT is rebuilt
// from the monotone Hermite spline in `geom/Spline.h` whenever the caller
// commits new points. Identity state is `[(0, 0), (1, 1)]` on every
// channel — default-construction is a zero-effect layer so insertion +
// later edit flows mirror LevelsAdjustment exactly.
class CurvesAdjustment : public AdjustmentLayer {
 public:
  using PointsArray = std::array<std::vector<SplinePoint>, 4>;

  CurvesAdjustment();

  // Replace one channel's control points and rebuild its cached LUT. Caller
  // is responsible for keeping the points sorted by x and anchoring sensible
  // endpoints; the dialog enforces both.
  void setPoints(CurvesChannel ch, std::vector<SplinePoint> pts);
  const std::vector<SplinePoint>& points(CurvesChannel ch) const;

  // Bulk replace all four channels. Used by LayerParamsCommand for
  // undo/redo in the same shape as `LevelsAdjustment::setAllParams`.
  void setAllPoints(const PointsArray& pts);
  const PointsArray& allPoints() const { return points_; }

  void applyToAccum(TileCoord tc, Rgba32F* accum) const override;

  const uint8_t* lut(CurvesChannel ch) const {
    return lut_[static_cast<int>(ch)];
  }

 private:
  void rebuildLut(int ch);

  PointsArray points_{};
  uint8_t lut_[4][256]{};
};

}  // namespace tuxels
