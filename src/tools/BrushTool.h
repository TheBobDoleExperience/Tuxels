#pragma once

#include <memory>
#include <optional>

#include "brush/RoundBrush.h"
#include "core/TuxImage.h"
#include "tools/ToolBase.h"

namespace tuxels {

class BrushEngine;
class LayerBase;

class BrushTool : public ToolBase {
 public:
  BrushTool();
  ~BrushTool() override;

  RoundBrush& brush() noexcept { return brush_; }
  const RoundBrush& brush() const noexcept { return brush_; }

  void press(Document& doc, float x, float y, MouseButton btn) override;
  void move(Document& doc, float x, float y) override;
  void release(Document& doc, float x, float y, MouseButton btn) override;
  Rect takeDirtyRect() override;
  std::optional<float> cursorRadiusPx() const override {
    // M11-S0: shrink the cursor ring to match the actual stamp footprint
    // when stylus pressure is < 1. Mouse + tablet-at-full-pressure paths
    // stay at the brush's nominal diameter (pressure() defaults 1.0).
    const float p = pressure();
    return brush_.diameter() * 0.5f * (p < 1.f ? p : 1.f);
  }

  struct StrokeInfo {
    LayerBase* layer = nullptr;
    TuxImage* target = nullptr;  // pixel layer image, or any layer's mask image
    Rect bounds;
    TuxImage::Recorded recorded;
  };
  const StrokeInfo& lastStroke() const noexcept { return last_; }
  StrokeInfo takeLastStroke() {
    StrokeInfo out = std::move(last_);
    last_ = {};
    return out;
  }

 private:
  RoundBrush brush_;
  LayerBase* active_ = nullptr;
  TuxImage* activeTarget_ = nullptr;
  std::unique_ptr<BrushEngine> engine_;
  StrokeInfo last_{};
};

}  // namespace tuxels
