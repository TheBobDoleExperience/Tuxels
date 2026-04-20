#pragma once

#include <optional>

#include "core/TuxImage.h"
#include "tools/ToolBase.h"

namespace tuxels {

class Document;

// Rubber-band crop tool. Press/move/release defines a rectangle in document
// coordinates; on release with a non-degenerate rect, the tool produces a
// PendingCrop for MainWindow to turn into a CropCommand. No interactive
// commit / cancel handles in this first cut — a drag is a commit; you can
// undo with Ctrl+Z if the rect was wrong.
class CropTool : public ToolBase {
 public:
  CropTool() = default;

  void press(Document& doc, float x, float y, MouseButton btn) override;
  void move(Document& doc, float x, float y) override;
  void release(Document& doc, float x, float y, MouseButton btn) override;

  std::optional<float> cursorRadiusPx() const override {
    return std::nullopt;
  }

  // Keep the rubber-band rendering shared with MarqueeTool via ToolBase.
  std::optional<Rect> liveRect() const override {
    if (!dragging_) return std::nullopt;
    return computeRect();
  }

  // Result of a completed drag. `rect` is the document-clipped crop
  // rectangle. MainWindow pulls this on release and builds a CropCommand.
  struct PendingCrop {
    Rect rect;
  };
  std::optional<PendingCrop> takeCommit() {
    if (!pending_) return std::nullopt;
    auto out = *pending_;
    pending_.reset();
    return out;
  }

 private:
  Rect computeRect() const;

  bool dragging_ = false;
  int startX_ = 0, startY_ = 0;
  int curX_ = 0, curY_ = 0;
  int docW_ = 0, docH_ = 0;
  std::optional<PendingCrop> pending_;
};

}  // namespace tuxels
