#pragma once

#include <string>

#include "core/Pixel.h"
#include "core/TuxImage.h"
#include "fill/FloodFill.h"
#include "tools/ToolBase.h"

namespace tuxels {

class Document;
class LayerBase;

// Paint-bucket tool. A single press triggers a scanline flood fill at the
// cursor pixel on the active PixelLayer's paint target, clipped to the
// active selection. Drag/release are no-ops — the tool emits its effect on
// press so per-click undo works naturally. The owning window pulls the
// tile-COW snapshot out via takeLastFill() to build a PaintCommand, the
// same undo pathway the brush uses.
class BucketTool : public ToolBase {
 public:
  BucketTool() = default;

  void press(Document& doc, float x, float y, MouseButton btn) override;
  void move(Document& /*doc*/, float /*x*/, float /*y*/) override {}
  void release(Document& /*doc*/, float /*x*/, float /*y*/,
               MouseButton /*btn*/) override {}

  Rect takeDirtyRect() override {
    Rect r = dirty_;
    dirty_ = {};
    return r;
  }

  // The bucket has no finite footprint preview (the fill region is
  // data-dependent), so no cursor ring.
  std::optional<float> cursorRadiusPx() const override { return std::nullopt; }

  void setColor(Rgba32F c) noexcept { color_ = c; }
  Rgba32F color() const noexcept { return color_; }

  void setTolerance(float t) noexcept { opts_.tolerance = t; }
  float tolerance() const noexcept { return opts_.tolerance; }

  void setOpacity(float o) noexcept { opts_.opacity = o; }
  float opacity() const noexcept { return opts_.opacity; }

  void setContiguous(bool v) noexcept { opts_.contiguous = v; }
  bool contiguous() const noexcept { return opts_.contiguous; }

  // Handed to MainWindow so it can push a PaintCommand onto the undo stack.
  // `target` and `layer` are null when the last press was a no-op (e.g.
  // clicked outside a valid paint target, or on a pixel fully outside the
  // selection).
  struct LastFill {
    LayerBase* layer = nullptr;
    TuxImage* target = nullptr;
    Rect bounds;
    TuxImage::Recorded recorded;
  };
  LastFill takeLastFill() {
    LastFill out = std::move(last_);
    last_ = {};
    return out;
  }

 private:
  Rgba32F color_{};
  FloodFillOptions opts_{};
  Rect dirty_{};
  LastFill last_{};
};

}  // namespace tuxels
