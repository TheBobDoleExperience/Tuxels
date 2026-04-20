#pragma once

#include <memory>
#include <optional>
#include <string>

#include "core/SelectionMask.h"
#include "core/TuxImage.h"
#include "tools/ToolBase.h"

namespace tuxels {

class Document;

// Rectangular marquee selection tool. Press sets the drag origin; move
// updates the live rectangle; release builds a fresh SelectionMask of the
// rect, combines it with the existing document selection per the modifiers
// captured at press time (Shift = Add, Alt = Subtract, Shift+Alt =
// Intersect, none = Replace), and stashes the before/after pair for the
// owning window to commit as a SelectionCommand.
class MarqueeTool : public ToolBase {
 public:
  MarqueeTool() = default;

  void press(Document& doc, float x, float y, MouseButton btn) override;
  void move(Document& doc, float x, float y) override;
  void release(Document& doc, float x, float y, MouseButton btn) override;

  bool consumesShiftClick() const override { return true; }
  std::optional<float> cursorRadiusPx() const override { return std::nullopt; }

  // Live rubber-band rectangle in document pixel coordinates, during a
  // drag. Empty when not dragging.
  std::optional<Rect> liveRect() const noexcept {
    if (!dragging_) return std::nullopt;
    return computeRect();
  }

  // Result of a completed drag, handed to the window so it can push a
  // SelectionCommand. `after` is nullptr when the combined result is empty
  // (collapse to null "no selection"). The window pulls this on release.
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

  // Persistent combine mode, driven by the marquee options in ToolsPanel.
  // Defaults to Replace. A drag uses this unless modifier keys are held at
  // press time, in which case modifiers win (so the Photoshop Shift/Alt
  // temporary-override still works on systems where the WM doesn't eat the
  // Alt key for window gestures).
  void setMode(SelectionMode m) noexcept { persistentMode_ = m; }
  SelectionMode mode() const noexcept { return persistentMode_; }

 private:
  Rect computeRect() const;
  static SelectionMode modeFromModifiers(int mods);

  bool dragging_ = false;
  int startX_ = 0, startY_ = 0;
  int curX_ = 0, curY_ = 0;
  int pressMods_ = 0;
  int docW_ = 0, docH_ = 0;
  SelectionMode persistentMode_ = SelectionMode::Replace;
  std::optional<PendingCommit> pending_;
};

}  // namespace tuxels
