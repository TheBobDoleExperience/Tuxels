#pragma once

#include <vector>

#include "core/TuxImage.h"
#include "layers/LayerBase.h"
#include "tools/ToolBase.h"

namespace tuxels {

class Document;

// Move-tool: drag the active (and any other multi-selected) pixel layers'
// origins around the canvas. Pixels are untouched — only `originX/originY`
// change, so each layer's backing image and any mask stay allocated
// exactly as they are (including pixels that move offscreen during the
// drag).
//
// M7-S0: when `Document::selectedLayerIds()` has more than one entry, all
// selected pixel layers move together by the same drag delta. Non-pixel
// layers in the selection (groups, adjustment layers) are ignored for
// movement — their child layers, if any, must be selected explicitly.
//
// Live drag writes the origin directly onto each captured layer so the
// existing paint path (takeDirtyRect → partial recomposite → widget
// update) renders the preview without new machinery. On release we latch
// a `PendingMove` per moved layer; MainWindow drains the vector and
// produces a single undo entry — `MoveLayerCommand` for the 1-layer case,
// `LayerOpCommand` whose closures iterate over the batch otherwise.
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

  // Per-layer origin diff produced by a finished drag.
  struct PendingMove {
    LayerId layerId = 0;
    int beforeX = 0;
    int beforeY = 0;
    int afterX = 0;
    int afterY = 0;
  };

  // Drain pending commits. Returns one entry per moved pixel layer (1 for
  // a single-active drag, N for a multi-select drag). Empty if nothing
  // moved or the drag was cancelled.
  std::vector<PendingMove> takeCommits() {
    auto out = std::move(pending_);
    pending_.clear();
    return out;
  }

 private:
  // Per-layer drag state captured at press(). Each entry tracks the
  // layer's id + original origin + dimensions used by the dirty-rect
  // union math so per-layer move deltas can union into one shared
  // `dirty_` rect.
  struct DragLayer {
    LayerId id = 0;
    int beforeX = 0;
    int beforeY = 0;
    int layerW = 0;
    int layerH = 0;
  };

  bool dragging_ = false;
  int startMouseX_ = 0;
  int startMouseY_ = 0;
  int docW_ = 0;
  int docH_ = 0;
  std::vector<DragLayer> dragLayers_;
  Rect dirty_{};
  std::vector<PendingMove> pending_;
};

}  // namespace tuxels
