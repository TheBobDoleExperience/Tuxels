#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/SelectionMask.h"
#include "tools/ToolBase.h"

namespace tuxels {

class Document;

// Freehand lasso. Press captures the start vertex; every subsequent move
// appends the cursor position to an in-progress polyline (with a 1-doc-px
// chord decimation so a slow drag doesn't balloon into thousands of
// near-duplicate points). Release closes the polygon, rasterizes it
// through `SelectionMask::fillPolygon`, combines with the existing
// document selection per the modifiers-at-press-time / persistent mode,
// and stashes a PendingCommit for the owning window to push as a
// SelectionCommand. Mirrors the MarqueeTool idiom.
class LassoTool : public ToolBase {
 public:
  LassoTool() = default;

  void press(Document& doc, float x, float y, MouseButton btn) override;
  void move(Document& doc, float x, float y) override;
  void release(Document& doc, float x, float y, MouseButton btn) override;

  bool consumesShiftClick() const override { return true; }

  std::optional<std::vector<Point2f>> livePath() const override {
    if (!dragging_ || points_.size() < 2) return std::nullopt;
    return points_;
  }

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

  // Persistent combine mode (Replace / Add / Subtract / Intersect) driven
  // by the options row in ToolsPanel. Modifiers held at press time still
  // override (Shift=Add, Alt=Subtract, Shift+Alt=Intersect) so the
  // Photoshop temporary override keeps working on WMs that don't grab
  // Alt-drag.
  void setMode(SelectionMode m) noexcept { persistentMode_ = m; }
  SelectionMode mode() const noexcept { return persistentMode_; }

 private:
  static SelectionMode modeFromModifiers(int mods);

  bool dragging_ = false;
  std::vector<Point2f> points_;
  int pressMods_ = 0;
  int docW_ = 0, docH_ = 0;
  SelectionMode persistentMode_ = SelectionMode::Replace;
  std::optional<PendingCommit> pending_;
};

}  // namespace tuxels
