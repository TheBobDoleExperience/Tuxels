#pragma once

#include <array>
#include <cstdint>

#include "layers/AdjustmentLayer.h"

namespace tuxels {

// Standard Photoshop-style Levels parameter set. Input range maps
// `[inBlack, inWhite]` → `[0, 1]` after a gamma curve, then remaps to
// `[outBlack, outWhite]`. Identity is {0, 1, 1, 0, 1}. Values are kept as
// float so the dialog can round-trip numerically; the per-apply cost is
// absorbed into a cached 256-entry LUT per channel (rebuilt on `setParams`).
struct LevelsParams {
  float inBlack = 0.f;
  float inWhite = 1.f;
  float gamma = 1.f;
  float outBlack = 0.f;
  float outWhite = 1.f;
};

// Channel selector. Composite applies to R/G/B identically (matches PS:
// composite Levels is a single params set acting on all three channels
// before the per-channel LUTs). R/G/B each carry their own LevelsParams
// and run after the composite pass.
enum class LevelsChannel : int {
  Composite = 0,
  R = 1,
  G = 2,
  B = 3,
};

class LevelsAdjustment : public AdjustmentLayer {
 public:
  LevelsAdjustment();

  // Replace one channel's params and rebuild its cached LUT. Thread-unsafe
  // with concurrent applyToAccum (caller serialises via the main thread).
  void setParams(LevelsChannel ch, const LevelsParams& p);
  const LevelsParams& params(LevelsChannel ch) const;

  // Bulk replace all four channels. Used by LayerParamsCommand for undo/redo.
  void setAllParams(const std::array<LevelsParams, 4>& p);
  const std::array<LevelsParams, 4>& allParams() const { return params_; }

  void applyToAccum(TileCoord tc, Rgba32F* accum) const override;

  // Direct LUT access for tests. Index 0 = composite, 1/2/3 = R/G/B.
  const uint8_t* lut(LevelsChannel ch) const {
    return lut_[static_cast<int>(ch)];
  }

 private:
  void rebuildLut(int ch);

  std::array<LevelsParams, 4> params_{};
  uint8_t lut_[4][256]{};
};

}  // namespace tuxels
