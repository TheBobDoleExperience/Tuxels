#include "tools/TransformTool.h"

#include <algorithm>
#include <cmath>

#include "core/Document.h"
#include "geom/Affine2D.h"
#include "geom/Resample.h"
#include "layers/PixelLayer.h"

namespace tuxels {

namespace {

// Doc-pixel radius within which a click counts as a corner-handle grab.
// Fixed for M2; future zoom-aware UI can override via CanvasView.
constexpr float kCornerHitRadius = 10.f;

// Doc-pixel radius for the pivot dot hit-test. Tested before corner so a
// click that lands inside the corner hit region but closer to the pivot (e.g.
// pivot moved onto a corner) still grabs the pivot.
constexpr float kPivotHitRadius = 8.f;

// 15° snap step used for Shift-rotate.
constexpr float kRotateSnapStep = 0.26179938779914943653855361527329f;  // π/12

// src→doc affine. Scale + rotation are centered on the pivot (px, py); the
// intermediate `translate(cx-px, cy-py)` re-inserts the offset between bbox
// center and pivot so that when `pivot == center` the transform reduces to
// the identity-pivot form `translate(-sw/2,-sh/2) · scale · rotate ·
// translate(cx, cy)`.
Affine2D buildSrcToDoc(float centerX, float centerY, float pivotX, float pivotY,
                       float scaleX, float scaleY, float angle, int srcW,
                       int srcH) {
  return Affine2D::translation(-srcW * 0.5f, -srcH * 0.5f)
      .then(Affine2D::translation(centerX - pivotX, centerY - pivotY))
      .then(Affine2D::scaling(scaleX, scaleY))
      .then(Affine2D::rotation(angle))
      .then(Affine2D::translation(pivotX, pivotY));
}

}  // namespace

bool TransformTool::enter(Document& doc) {
  LayerBase* base = doc.activeLayer();
  if (!base) return false;
  auto* px = dynamic_cast<PixelLayer*>(base);
  if (!px) return false;

  active_ = true;
  layerId_ = base->id;
  src_ = px->image;  // shared_ptr-backed tiles → cheap copy.
  srcW_ = px->image.width();
  srcH_ = px->image.height();
  srcOriginX_ = base->originX;
  srcOriginY_ = base->originY;
  centerX_ = srcOriginX_ + srcW_ * 0.5f;
  centerY_ = srcOriginY_ + srcH_ * 0.5f;
  scaleX_ = 1.f;
  scaleY_ = 1.f;
  angle_ = 0.f;
  pivotX_ = centerX_;
  pivotY_ = centerY_;
  docW_ = doc.width();
  docH_ = doc.height();
  dragging_ = false;
  dragMode_ = DragMode::None;
  dragCorner_ = -1;
  rebuildScratch();
  markWholeDocDirty(doc);
  return true;
}

void TransformTool::cancel() {
  active_ = false;
  layerId_ = 0;
  dragging_ = false;
  dragMode_ = DragMode::None;
  src_ = TuxImage();
  scratch_ = TuxImage();
}

std::optional<TransformTool::PendingCommit> TransformTool::commit() {
  if (!active_) return std::nullopt;
  // Identity-transform commit is a no-op.
  if (scaleX_ == 1.f && scaleY_ == 1.f && angle_ == 0.f &&
      centerX_ == srcOriginX_ + srcW_ * 0.5f &&
      centerY_ == srcOriginY_ + srcH_ * 0.5f) {
    active_ = false;
    return std::nullopt;
  }

  PendingCommit p;
  p.layerId = layerId_;
  p.before = src_;
  p.after = scratch_;
  p.beforeX = srcOriginX_;
  p.beforeY = srcOriginY_;
  p.afterX = scratchOriginX_;
  p.afterY = scratchOriginY_;
  active_ = false;
  return p;
}

void TransformTool::setTranslation(float cx, float cy) {
  if (!active_) return;
  const float dx = cx - centerX_;
  const float dy = cy - centerY_;
  centerX_ = cx;
  centerY_ = cy;
  pivotX_ += dx;
  pivotY_ += dy;
  rebuildScratch();
}

void TransformTool::setScale(float sx, float sy) {
  if (!active_) return;
  // Guard against zero/negative scale — collapses to nothing visible.
  constexpr float kMinScale = 0.01f;
  scaleX_ = std::fabs(sx) < kMinScale ? (sx < 0.f ? -kMinScale : kMinScale) : sx;
  scaleY_ = std::fabs(sy) < kMinScale ? (sy < 0.f ? -kMinScale : kMinScale) : sy;
  rebuildScratch();
}

void TransformTool::setRotation(float radians) {
  if (!active_) return;
  angle_ = radians;
  rebuildScratch();
}

std::array<std::array<float, 2>, 4> TransformTool::computeCorners() const {
  const Affine2D m = buildSrcToDoc(centerX_, centerY_, pivotX_, pivotY_,
                                    scaleX_, scaleY_, angle_, srcW_, srcH_);
  std::array<std::array<float, 2>, 4> c;
  const float fW = static_cast<float>(srcW_);
  const float fH = static_cast<float>(srcH_);
  const float pts[4][2] = {{0.f, 0.f}, {fW, 0.f}, {fW, fH}, {0.f, fH}};
  for (int i = 0; i < 4; ++i) {
    m.mapPoint(pts[i][0], pts[i][1], c[i][0], c[i][1]);
  }
  return c;
}

bool TransformTool::pointInQuad(
    float x, float y, const std::array<std::array<float, 2>, 4>& q) const {
  // Convex-quad test: point is inside iff cross products of consecutive
  // edges with (point - vertex) all share the same sign. Handles CW and
  // CCW winding (rotation flips the sign of all crosses uniformly).
  int sign = 0;
  for (int i = 0; i < 4; ++i) {
    const int j = (i + 1) % 4;
    const float ex = q[j][0] - q[i][0];
    const float ey = q[j][1] - q[i][1];
    const float dx = x - q[i][0];
    const float dy = y - q[i][1];
    const float cross = ex * dy - ey * dx;
    if (cross > 0.f) {
      if (sign < 0) return false;
      sign = 1;
    } else if (cross < 0.f) {
      if (sign > 0) return false;
      sign = -1;
    }
  }
  return true;
}

int TransformTool::nearestCornerWithin(float x, float y,
                                       float maxDocDist) const {
  const auto corners = computeCorners();
  int best = -1;
  float bestD2 = maxDocDist * maxDocDist;
  for (int i = 0; i < 4; ++i) {
    const float dx = x - corners[i][0];
    const float dy = y - corners[i][1];
    const float d2 = dx * dx + dy * dy;
    if (d2 <= bestD2) {
      bestD2 = d2;
      best = i;
    }
  }
  return best;
}

void TransformTool::rebuildScratch() {
  if (!active_) return;
  const auto corners = computeCorners();

  float minX = corners[0][0], maxX = corners[0][0];
  float minY = corners[0][1], maxY = corners[0][1];
  for (int i = 1; i < 4; ++i) {
    minX = std::min(minX, corners[i][0]);
    maxX = std::max(maxX, corners[i][0]);
    minY = std::min(minY, corners[i][1]);
    maxY = std::max(maxY, corners[i][1]);
  }
  const int ox = static_cast<int>(std::floor(minX));
  const int oy = static_cast<int>(std::floor(minY));
  const int w = std::max(1, static_cast<int>(std::ceil(maxX)) - ox);
  const int h = std::max(1, static_cast<int>(std::ceil(maxY)) - oy);

  scratchOriginX_ = ox;
  scratchOriginY_ = oy;
  scratch_ = TuxImage(w, h);

  const Affine2D srcToDoc = buildSrcToDoc(centerX_, centerY_, pivotX_, pivotY_,
                                           scaleX_, scaleY_, angle_, srcW_,
                                           srcH_);
  const Affine2D dstLocalToSrc =
      Affine2D::translation(static_cast<float>(ox), static_cast<float>(oy))
          .then(srcToDoc.inverse());
  resampleBilinear(src_, scratch_, dstLocalToSrc);
}

void TransformTool::markWholeDocDirty(const Document& doc) {
  dirty_ = {0, 0, doc.width(), doc.height()};
}

TransformTool::Overlay TransformTool::overlay() const {
  Overlay o;
  o.active = active_;
  if (active_) {
    o.layer.layerId = layerId_;
    o.layer.image = &scratch_;
    o.layer.originX = scratchOriginX_;
    o.layer.originY = scratchOriginY_;
    o.corners = computeCorners();
    o.pivot = {pivotX_, pivotY_};
  }
  return o;
}

void TransformTool::press(Document& doc, float x, float y, MouseButton btn) {
  if (!active_ || btn != MouseButton::Left) return;

  // Pivot hit-test runs before corner so the pivot stays grabbable when it
  // drifts close to a corner.
  const float dpx = x - pivotX_;
  const float dpy = y - pivotY_;
  const bool hitPivot = (dpx * dpx + dpy * dpy) <=
                        (kPivotHitRadius * kPivotHitRadius);

  DragMode mode;
  int corner = -1;
  if (hitPivot) {
    mode = DragMode::Pivot;
  } else {
    corner = nearestCornerWithin(x, y, kCornerHitRadius);
    const auto q = computeCorners();
    if (corner >= 0) {
      mode = DragMode::Scale;
    } else if (pointInQuad(x, y, q)) {
      mode = DragMode::Translate;
    } else {
      mode = DragMode::Rotate;
    }
  }

  dragging_ = true;
  dragMode_ = mode;
  dragCorner_ = corner;
  dragStartX_ = x;
  dragStartY_ = y;
  dragStartCenterX_ = centerX_;
  dragStartCenterY_ = centerY_;
  dragStartAngle_ = angle_;
  dragStartScaleX_ = scaleX_;
  dragStartScaleY_ = scaleY_;
  dragStartPivotX_ = pivotX_;
  dragStartPivotY_ = pivotY_;

  // For Scale: capture the grabbed corner's pre-scale local coords in the
  // pre-rotation frame. `local = (src_corner - srcCenter) + (center - pivot)`.
  // newScale = unrotated(cursor - pivot) / local.
  if (mode == DragMode::Scale && corner >= 0) {
    const float kx = dragStartCenterX_ - dragStartPivotX_ - srcW_ * 0.5f;
    const float ky = dragStartCenterY_ - dragStartPivotY_ - srcH_ * 0.5f;
    const float cornerOffsetX[4] = {0.f, static_cast<float>(srcW_),
                                     static_cast<float>(srcW_), 0.f};
    const float cornerOffsetY[4] = {0.f, 0.f, static_cast<float>(srcH_),
                                     static_cast<float>(srcH_)};
    dragStartLocalX_ = kx + cornerOffsetX[corner];
    dragStartLocalY_ = ky + cornerOffsetY[corner];
  } else {
    dragStartLocalX_ = 0.f;
    dragStartLocalY_ = 0.f;
  }

  markWholeDocDirty(doc);
}

void TransformTool::move(Document& doc, float x, float y) {
  if (!active_ || !dragging_) return;
  const float dx = x - dragStartX_;
  const float dy = y - dragStartY_;
  const bool shift = (modifiers_ & Mod::Shift) != 0;

  switch (dragMode_) {
    case DragMode::Translate: {
      centerX_ = dragStartCenterX_ + dx;
      centerY_ = dragStartCenterY_ + dy;
      // Pivot rides along with the whole transform on translate.
      pivotX_ = dragStartPivotX_ + dx;
      pivotY_ = dragStartPivotY_ + dy;
      break;
    }
    case DragMode::Rotate: {
      // Angle reference is the pivot (not the bbox center) so rotation
      // feels anchored at the visible handle. Shift snaps the *delta*, so a
      // drag that started at a non-multiple stays consistent.
      const float a0 = std::atan2(dragStartY_ - dragStartPivotY_,
                                   dragStartX_ - dragStartPivotX_);
      const float a1 =
          std::atan2(y - dragStartPivotY_, x - dragStartPivotX_);
      float delta = a1 - a0;
      if (shift) {
        delta = std::round(delta / kRotateSnapStep) * kRotateSnapStep;
      }
      angle_ = dragStartAngle_ + delta;
      break;
    }
    case DragMode::Scale: {
      if (dragCorner_ < 0) break;
      // Un-rotate the doc-space vector (cursor - pivot) back into the
      // pre-rotation frame; divide by the grabbed corner's captured local
      // coords to get the new per-axis scale.
      const float c = std::cos(dragStartAngle_);
      const float s = std::sin(dragStartAngle_);
      const float vx = x - dragStartPivotX_;
      const float vy = y - dragStartPivotY_;
      const float ux = c * vx + s * vy;
      const float uy = -s * vx + c * vy;
      constexpr float kMinLocal = 0.5f;
      const float lx =
          std::fabs(dragStartLocalX_) < kMinLocal
              ? std::copysign(kMinLocal, dragStartLocalX_ == 0.f ? 1.f
                                                               : dragStartLocalX_)
              : dragStartLocalX_;
      const float ly =
          std::fabs(dragStartLocalY_) < kMinLocal
              ? std::copysign(kMinLocal, dragStartLocalY_ == 0.f ? 1.f
                                                               : dragStartLocalY_)
              : dragStartLocalY_;
      float newSx = ux / lx;
      float newSy = uy / ly;
      constexpr float kMinScale = 0.01f;
      if (std::fabs(newSx) < kMinScale)
        newSx = (newSx < 0.f ? -kMinScale : kMinScale);
      if (std::fabs(newSy) < kMinScale)
        newSy = (newSy < 0.f ? -kMinScale : kMinScale);
      if (shift) {
        // Shift locks Y magnitude to X; Y sign is preserved so flipping
        // across the pivot still works.
        newSy = std::copysign(std::fabs(newSx), newSy);
      }
      scaleX_ = newSx;
      scaleY_ = newSy;
      break;
    }
    case DragMode::Pivot: {
      // Pivot drag moves only `pivotX_/Y_`; scale/rotate/center are
      // untouched. rebuildScratch picks up the new rotation center on the
      // next mouse move after release.
      pivotX_ = dragStartPivotX_ + dx;
      pivotY_ = dragStartPivotY_ + dy;
      break;
    }
    case DragMode::None:
      break;
  }
  rebuildScratch();
  (void)doc;
  dirty_ = {0, 0, docW_, docH_};
}

void TransformTool::release(Document&, float, float, MouseButton btn) {
  if (!active_ || btn != MouseButton::Left) return;
  dragging_ = false;
  dragMode_ = DragMode::None;
  dragCorner_ = -1;
}

}  // namespace tuxels
