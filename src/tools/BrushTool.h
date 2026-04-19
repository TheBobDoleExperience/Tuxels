#pragma once

#include <memory>

#include "brush/RoundBrush.h"
#include "core/TuxImage.h"
#include "tools/ToolBase.h"

namespace tuxels {

class BrushEngine;
class PixelLayer;

class BrushTool : public ToolBase {
 public:
  BrushTool();
  ~BrushTool() override;

  RoundBrush& brush() noexcept { return brush_; }
  const RoundBrush& brush() const noexcept { return brush_; }

  void press(Document& doc, float x, float y, MouseButton btn) override;
  void move(Document& doc, float x, float y) override;
  void release(Document& doc, float x, float y, MouseButton btn) override;

  struct StrokeInfo {
    PixelLayer* layer = nullptr;
    Rect bounds;
  };
  StrokeInfo lastStroke() const noexcept { return last_; }

 private:
  RoundBrush brush_;
  PixelLayer* active_ = nullptr;
  std::unique_ptr<BrushEngine> engine_;
  StrokeInfo last_{};
};

}  // namespace tuxels
