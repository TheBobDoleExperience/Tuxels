#pragma once

#include <array>
#include <optional>
#include <vector>

#include "compositor/compose.h"
#include "core/TuxImage.h"
#include "layers/LayerBase.h"
#include "tools/ToolBase.h"

namespace tuxels {

class Document;

// Modal Free-Transform tool. M10-S1: now multi-source — collects every
// PixelLayer from `Document::selectedLayerIds()` (falls back to the active
// layer when no multi-selection is active). All sources transform around
// a SHARED bbox-union frame in doc coords; per-source scratch + override
// during the live drag; commit returns one PendingCommit per source.
//
// enter() captures all pixel sources, computes the bbox-union, and seeds
// an identity transform centered on the union's center. While the tool
// is active, translation/rotation/uniform-scale exposed via direct
// setters and drag gestures (corners, interior, exterior, pivot) — same
// gesture vocabulary as the single-source M2 version.
//
// Backward compat: tests + the existing `Overlay::layer` / `commit()`
// surface keep working when there's exactly one source. New
// `Overlay::overrides` / `commits()` expose the full vector.
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
    // Legacy: first source's override (kept for the existing test surface).
    LayerOverride layer;
    // Full set of overrides — one per source. Length matches `sources_`.
    std::vector<LayerOverride> overrides;
    // Bbox-union corners after the current transform, in doc coords.
    // Order: TL, TR, BR, BL.
    std::array<std::array<float, 2>, 4> corners{};
    // Rotation/scale pivot in doc coords.
    std::array<float, 2> pivot{};
  };

  TransformTool() = default;

  bool enter(Document& doc);
  // Legacy commit: returns nullopt if no sources, the first PendingCommit
  // otherwise. Use `commits()` to drain all sources.
  std::optional<PendingCommit> commit();
  std::vector<PendingCommit> commits();
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
  float pivotX() const noexcept { return pivotX_; }
  float pivotY() const noexcept { return pivotY_; }
  std::size_t sourceCount() const noexcept { return sources_.size(); }

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
  enum class DragMode { None, Translate, Rotate, Scale, Pivot };

  // Per-layer source state captured at enter().
  struct Source {
    LayerId layerId = 0;
    TuxImage src;
    int srcOriginX = 0;
    int srcOriginY = 0;
    int srcW = 0;
    int srcH = 0;
    TuxImage scratch;
    int scratchOriginX = 0;
    int scratchOriginY = 0;
  };

  void rebuildScratch();
  void rebuildScratchFor(Source& s) const;
  void markWholeDocDirty(const Document& doc);
  std::array<std::array<float, 2>, 4> computeOuterCorners() const;
  bool pointInQuad(float x, float y,
                   const std::array<std::array<float, 2>, 4>& q) const;
  int nearestCornerWithin(float x, float y, float maxDocDist) const;

  bool active_ = false;
  std::vector<Source> sources_;

  // Bbox-union of all sources' content rects in doc coords, captured at
  // enter() time. Constant for the lifetime of the modal session.
  int outerOriginX_ = 0;
  int outerOriginY_ = 0;
  int outerW_ = 0;
  int outerH_ = 0;

  // Transform state, in doc coords. Centered on the bbox-union center
  // initially.
  float centerX_ = 0.f;
  float centerY_ = 0.f;
  float scaleX_ = 1.f;
  float scaleY_ = 1.f;
  float angle_ = 0.f;
  float pivotX_ = 0.f;
  float pivotY_ = 0.f;

  // Drag state.
  bool dragging_ = false;
  DragMode dragMode_ = DragMode::None;
  int dragCorner_ = -1;
  float dragStartX_ = 0.f;
  float dragStartY_ = 0.f;
  float dragStartCenterX_ = 0.f;
  float dragStartCenterY_ = 0.f;
  float dragStartAngle_ = 0.f;
  float dragStartScaleX_ = 1.f;
  float dragStartScaleY_ = 1.f;
  float dragStartPivotX_ = 0.f;
  float dragStartPivotY_ = 0.f;
  // For Scale drags: the grabbed corner's pre-scale local coords (pre-
  // rotation frame, relative to pivot). newScale = u / dragStartLocal.
  float dragStartLocalX_ = 0.f;
  float dragStartLocalY_ = 0.f;

  Rect dirty_;
  int docW_ = 0;
  int docH_ = 0;
};

}  // namespace tuxels
