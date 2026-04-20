#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/SelectionMask.h"
#include "tools/ToolBase.h"

namespace tuxels {

class Document;

// Polygonal lasso. Each left-click appends a vertex; `hover()` tracks the
// cursor between clicks so CanvasView can draw a rubber-band last edge
// from the most recent vertex to the current cursor position. The
// polygon closes when the user clicks within `closeRadiusPx` of the
// starting vertex (with ≥3 total) or when `finish()` is called
// externally (Enter key in MainWindow). `cancel()` discards the
// in-progress polygon without committing (Escape).
//
// Unlike LassoTool / MarqueeTool, release is a no-op — the tool is
// driven by discrete presses, not drags. State persists across press/
// release boundaries until commit or cancel.
class PolyLassoTool : public ToolBase {
 public:
  PolyLassoTool() = default;

  void press(Document& doc, float x, float y, MouseButton btn) override;
  void move(Document& doc, float x, float y) override;
  void release(Document& doc, float x, float y, MouseButton btn) override;
  void hover(Document& doc, float x, float y) override;

  bool consumesShiftClick() const override { return true; }

  std::optional<std::vector<Point2f>> livePath() const override {
    if (!building_) return std::nullopt;
    // Emit points + current cursor so CanvasView renders a trailing
    // rubber-band edge from the last vertex to where the cursor is now.
    auto p = points_;
    p.push_back(cursor_);
    return p;
  }

  // Accept/close the in-progress polygon (Enter key from MainWindow).
  // Builds the PendingCommit if the polygon has ≥3 vertices.
  void finish(Document& doc);
  // Discard the in-progress polygon without committing (Escape key).
  void cancel();
  bool isBuilding() const noexcept { return building_; }

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

  // Persistent combine mode (same semantics as LassoTool).
  void setMode(SelectionMode m) noexcept { persistentMode_ = m; }
  SelectionMode mode() const noexcept { return persistentMode_; }

 private:
  static SelectionMode modeFromModifiers(int mods);
  void buildCommit(Document& doc);

  static constexpr float kCloseRadiusPx = 6.f;  // doc-space px

  bool building_ = false;
  std::vector<Point2f> points_;
  Point2f cursor_{};
  int pressMods_ = 0;
  int docW_ = 0, docH_ = 0;
  SelectionMode persistentMode_ = SelectionMode::Replace;
  std::optional<PendingCommit> pending_;
};

}  // namespace tuxels
