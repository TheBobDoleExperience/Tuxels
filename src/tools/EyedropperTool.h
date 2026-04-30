#pragma once

#include <functional>
#include <optional>

#include "core/Pixel.h"
#include "tools/ToolBase.h"

namespace tuxels {

// Color picker. Single-click samples a doc-coord pixel from the cached
// composite (the canvas's most recent compose() output) and hands it off
// to a user-installed callback. Mouse drags are no-ops; only press
// matters. M11-S1.
class EyedropperTool : public ToolBase {
 public:
  using PickFn = std::function<void(float docX, float docY)>;

  EyedropperTool() = default;

  // Wire the callback that fires on every left-button press. Typically
  // MainWindow installs a closure that reads `CanvasView::sampleComposite`
  // and pushes the result into ToolsPanel::setForegroundColor.
  void setOnPick(PickFn cb) { onPick_ = std::move(cb); }

  void press(Document& /*doc*/, float x, float y, MouseButton btn) override {
    if (btn != MouseButton::Left || !onPick_) return;
    onPick_(x, y);
  }
  void move(Document& /*doc*/, float /*x*/, float /*y*/) override {}
  void release(Document& /*doc*/, float /*x*/, float /*y*/,
               MouseButton /*btn*/) override {}

  std::optional<float> cursorRadiusPx() const override { return std::nullopt; }

 private:
  PickFn onPick_;
};

}  // namespace tuxels
