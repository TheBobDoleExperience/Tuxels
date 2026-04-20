#pragma once

#include <optional>

#include "core/TuxImage.h"
#include "layers/LayerBase.h"
#include "tools/ToolBase.h"

namespace tuxels {

class Document;

// Move-tool: drag the active layer's origin around the canvas. Pixels are
// untouched — only `originX/originY` change, so the layer's backing image
// and any mask stay allocated exactly as they are (including pixels that
// move offscreen during the drag).
//
// Live drag writes the origin directly onto the layer so the existing
// paint path (takeDirtyRect → partial recomposite → widget update) can
// render the preview without any new machinery. On release we produce a
// `PendingMove` that MainWindow turns into a `MoveLayerCommand`; the
// command's apply() is idempotent w.r.t. the already-live origin, so
// pushing it immediately after the drag records the (before, after) pair
// without double-applying.
class MoveTool : public ToolBase {
 public:
  MoveTool() = default;

  void press(Document& doc, float x, float y, MouseButton btn) override;
  void move(Document& doc, float x, float y) override;
  void release(Document& doc, float x, float y, MouseButton btn) override;

  Rect takeDirtyRect() override {
    Rect r = dirty_;
    dirty_ = {};
    return r;
  }

  // Move has no brush-style finite footprint; no ring to draw.
  std::optional<float> cursorRadiusPx() const override { return std::nullopt; }

  // MainWindow polls this after release to push an undo entry. `layerId`
  // lets the command find the right layer even if layer ordering changes
  // between the commit and a later redo.
  struct PendingMove {
    LayerId layerId = 0;
    int beforeX = 0;
    int beforeY = 0;
    int afterX = 0;
    int afterY = 0;
  };
  std::optional<PendingMove> takeCommit() {
    if (!pending_) return std::nullopt;
    auto out = *pending_;
    pending_.reset();
    return out;
  }

 private:
  bool dragging_ = false;
  LayerId layerId_ = 0;
  int startMouseX_ = 0;
  int startMouseY_ = 0;
  int beforeX_ = 0;
  int beforeY_ = 0;
  int layerW_ = 0;
  int layerH_ = 0;
  int docW_ = 0;
  int docH_ = 0;
  Rect dirty_{};
  std::optional<PendingMove> pending_;
};

}  // namespace tuxels
