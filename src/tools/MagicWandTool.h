#pragma once

#include <memory>
#include <optional>
#include <string>

#include "core/SelectionMask.h"
#include "core/TuxImage.h"
#include "tools/ToolBase.h"

namespace tuxels {

class Document;

// Color-based contiguous selection ("magic wand"). A press at (x,y) samples
// the active layer's pixel color, runs a scanline flood from that seed
// using an L∞ tolerance, and builds a SelectionMask of the connected
// region. The mask is combined with the existing selection per the combine
// mode — same Replace/Add/Subtract/Intersect semantics as MarqueeTool and
// with the same two sources (modifiers at press time override the
// persistent mode set in the ToolsPanel options row).
//
// Drag / move / release are no-ops; the gesture is a single click.
class MagicWandTool : public ToolBase {
 public:
  MagicWandTool() = default;

  void press(Document& doc, float x, float y, MouseButton btn) override;
  void move(Document& /*doc*/, float /*x*/, float /*y*/) override {}
  void release(Document& /*doc*/, float /*x*/, float /*y*/,
               MouseButton /*btn*/) override {}

  // Pressed Shift on the canvas is "add to selection" — same as marquee.
  bool consumesShiftClick() const override { return true; }
  std::optional<float> cursorRadiusPx() const override { return std::nullopt; }

  void setTolerance(float t) noexcept { tolerance_ = t; }
  float tolerance() const noexcept { return tolerance_; }

  void setMode(SelectionMode m) noexcept { persistentMode_ = m; }
  SelectionMode mode() const noexcept { return persistentMode_; }

  // Same contract as MarqueeTool::PendingCommit — consumed by MainWindow on
  // the next `layerPainted` signal to build a SelectionCommand.
  struct PendingCommit {
    std::unique_ptr<SelectionMask> before;
    std::unique_ptr<SelectionMask> after;
    std::string label;
  };
  std::optional<PendingCommit> takeCommit() {
    if (!pending_) return std::nullopt;
    auto out = std::move(*pending_);
    pending_.reset();
    return out;
  }

 private:
  static SelectionMode modeFromModifiers(int mods);

  float tolerance_ = 0.f;
  SelectionMode persistentMode_ = SelectionMode::Replace;
  std::optional<PendingCommit> pending_;
};

}  // namespace tuxels
