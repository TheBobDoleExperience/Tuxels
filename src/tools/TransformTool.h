#pragma once

#include <array>
#include <optional>

#include "compositor/compose.h"
#include "core/TuxImage.h"
#include "layers/LayerBase.h"
#include "tools/ToolBase.h"

namespace tuxels {

class Document;

// Modal Free-Transform tool.
//
// enter() latches the active pixel layer as the transform source and
// initializes an identity transform centered on the layer's doc bbox.
// While the tool is active, translation/rotation/uniform-scale around the
// bbox center are exposed both as direct setters (for tests and keyboard
// nudges later) and as drag gestures against 4 corner handles (scale), the
// bbox interior (translate), and the region outside the bbox (rotate).
//
// The live preview is produced by resampling the captured source into a
// `scratch_` image sized to the transformed doc-bbox AABB, served via
// `overlay()` for CanvasView to feed to compose() as a LayerOverride.
//
// commit() returns a PendingCommit with the before/after TuxImages and
// origins that MainWindow wraps in a TransformCommand. cancel() discards
// the in-progress transform without touching the real layer.
class TransformTool : public ToolBase {
 public:
  struct PendingCommit {
    LayerId layerId = 0;
    TuxImage before;
    TuxImage after;
    int beforeX = 0;
    int beforeY = 0;
    int afterX = 0;
    int afterY = 0;
  };

  struct Overlay {
    bool active = false;
    LayerOverride layer;
    // Corners in doc-space, order TL, TR, BR, BL. Used by CanvasView to
    // draw the bbox + handle dots.
    std::array<std::array<float, 2>, 4> corners{};
  };

  TransformTool() = default;

  bool enter(Document& doc);
  std::optional<PendingCommit> commit();
  void cancel();
  bool isActive() const noexcept { return active_; }

  // Direct state setters (used by tests and future keyboard nudges).
  void setTranslation(float cx, float cy);
  void setScale(float sx, float sy);
  void setRotation(float radians);

  // Introspection.
  float centerX() const noexcept { return centerX_; }
  float centerY() const noexcept { return centerY_; }
  float scaleX() const noexcept { return scaleX_; }
  float scaleY() const noexcept { return scaleY_; }
  float rotation() const noexcept { return angle_; }

  Overlay overlay() const;

  // ToolBase:
  void press(Document& doc, float x, float y, MouseButton btn) override;
  void move(Document& doc, float x, float y) override;
  void release(Document& doc, float x, float y, MouseButton btn) override;
  Rect takeDirtyRect() override {
    Rect r = dirty_;
    dirty_ = {};
    return r;
  }
  std::optional<float> cursorRadiusPx() const override { return std::nullopt; }

 private:
  enum class DragMode { None, Translate, Rotate, Scale };

  void rebuildScratch();
  void markWholeDocDirty(const Document& doc);
  std::array<std::array<float, 2>, 4> computeCorners() const;
  bool pointInQuad(float x, float y,
                   const std::array<std::array<float, 2>, 4>& q) const;
  int nearestCornerWithin(float x, float y, float maxDocDist) const;

  bool active_ = false;
  LayerId layerId_ = 0;
  TuxImage src_;
  int srcW_ = 0;
  int srcH_ = 0;
  int srcOriginX_ = 0;
  int srcOriginY_ = 0;

  // Transform state, always interpreted relative to srcCenter=(srcW/2,srcH/2).
  float centerX_ = 0.f;
  float centerY_ = 0.f;
  float scaleX_ = 1.f;
  float scaleY_ = 1.f;
  float angle_ = 0.f;

  // Scratch preview + its doc-space origin.
  TuxImage scratch_;
  int scratchOriginX_ = 0;
  int scratchOriginY_ = 0;

  // Drag state.
  bool dragging_ = false;
  DragMode dragMode_ = DragMode::None;
  int dragCorner_ = -1;  // which of the 4 corners, for scale.
  float dragStartX_ = 0.f;
  float dragStartY_ = 0.f;
  float dragStartCenterX_ = 0.f;
  float dragStartCenterY_ = 0.f;
  float dragStartAngle_ = 0.f;
  float dragStartScaleX_ = 1.f;
  float dragStartScaleY_ = 1.f;

  Rect dirty_;
  int docW_ = 0;
  int docH_ = 0;
};

}  // namespace tuxels
