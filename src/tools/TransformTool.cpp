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
constexpr float kCornerHitRadius = 10.f;
// Doc-pixel radius for the pivot dot hit-test. Tested before corner so a
// click that lands inside the corner hit region but closer to the pivot
// still grabs the pivot.
constexpr float kPivotHitRadius = 8.f;
// 15° snap step used for Shift-rotate.
constexpr float kRotateSnapStep = 0.26179938779914943653855361527329f;  // π/12

// Build the doc → doc transform that the user's gesture applies to every
// source uniformly. The transform pivots scale + rotation around `pivot`
// and adds a translation so that the bbox-union's original center moves to
// `(centerX, centerY)`. At identity (centerX == bboxCenterInit, pivot ==
// bboxCenterInit, scale == 1, angle == 0) this reduces to the identity
// matrix — verified in the unit tests.
//
// Decomposition (right-to-left, since `then` left-multiplies in this
// codebase): we want
//
//   doc' = R · S · (doc - pivot) + pivot + (centerX - bboxCenter, centerY -
//   bboxCenter)
//
// which is equivalent to
//
//   doc' = R · S · (doc - bboxCenter + (bboxCenter - pivot)) + pivot +
//         (centerX - bboxCenter, ...)
//
// expressed as the chain below.
Affine2D buildDocToDoc(float centerX, float centerY, float pivotX,
                       float pivotY, float scaleX, float scaleY, float angle,
                       float bboxCenterX, float bboxCenterY) {
  return Affine2D::translation(-bboxCenterX, -bboxCenterY)
      .then(Affine2D::translation(bboxCenterX - pivotX, bboxCenterY - pivotY))
      .then(Affine2D::scaling(scaleX, scaleY))
      .then(Affine2D::rotation(angle))
      .then(Affine2D::translation(pivotX, pivotY))
      .then(Affine2D::translation(centerX - bboxCenterX,
                                   centerY - bboxCenterY));
}

}  // namespace

bool TransformTool::enter(Document& doc) {
  // M10-S1: collect every PixelLayer from the multi-selection set;
  // fall back to active when no multi-select is active. Non-pixel
  // layers (groups, adjustments) are skipped silently — they have no
  // backing image to transform.
  sources_.clear();
  std::vector<LayerBase*> candidates;
  const auto& selIds = doc.selectedLayerIds();
  if (!selIds.empty()) {
    for (LayerId id : selIds) {
      if (auto* l = doc.tree().findById(id)) candidates.push_back(l);
    }
  }
  if (candidates.empty()) {
    if (auto* a = doc.activeLayer()) candidates.push_back(a);
  }

  for (LayerBase* l : candidates) {
    auto* px = dynamic_cast<PixelLayer*>(l);
    if (!px) continue;
    Source s;
    s.layerId = l->id;
    s.src = px->image;  // shared_ptr-backed tiles → cheap copy
    s.srcW = px->image.width();
    s.srcH = px->image.height();
    s.srcOriginX = l->originX;
    s.srcOriginY = l->originY;
    sources_.push_back(std::move(s));
  }
  if (sources_.empty()) return false;

  // Bbox-union of all sources' content rects in doc coords.
  int minX = sources_[0].srcOriginX;
  int minY = sources_[0].srcOriginY;
  int maxX = sources_[0].srcOriginX + sources_[0].srcW;
  int maxY = sources_[0].srcOriginY + sources_[0].srcH;
  for (std::size_t i = 1; i < sources_.size(); ++i) {
    const Source& s = sources_[i];
    minX = std::min(minX, s.srcOriginX);
    minY = std::min(minY, s.srcOriginY);
    maxX = std::max(maxX, s.srcOriginX + s.srcW);
    maxY = std::max(maxY, s.srcOriginY + s.srcH);
  }
  outerOriginX_ = minX;
  outerOriginY_ = minY;
  outerW_ = std::max(1, maxX - minX);
  outerH_ = std::max(1, maxY - minY);

  centerX_ = outerOriginX_ + outerW_ * 0.5f;
  centerY_ = outerOriginY_ + outerH_ * 0.5f;
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
  active_ = true;
  rebuildScratch();
  markWholeDocDirty(doc);
  return true;
}

void TransformTool::cancel() {
  active_ = false;
  dragging_ = false;
  dragMode_ = DragMode::None;
  sources_.clear();
}

std::vector<TransformTool::PendingCommit> TransformTool::commits() {
  if (!active_) return {};
  // Identity-transform commit is a no-op for every source.
  const float bboxCx = outerOriginX_ + outerW_ * 0.5f;
  const float bboxCy = outerOriginY_ + outerH_ * 0.5f;
  if (scaleX_ == 1.f && scaleY_ == 1.f && angle_ == 0.f &&
      centerX_ == bboxCx && centerY_ == bboxCy) {
    active_ = false;
    return {};
  }
  std::vector<PendingCommit> out;
  out.reserve(sources_.size());
  for (auto& s : sources_) {
    PendingCommit p;
    p.layerId = s.layerId;
    p.before = s.src;
    p.after = s.scratch;
    p.beforeX = s.srcOriginX;
    p.beforeY = s.srcOriginY;
    p.afterX = s.scratchOriginX;
    p.afterY = s.scratchOriginY;
    out.push_back(std::move(p));
  }
  active_ = false;
  return out;
}

std::optional<TransformTool::PendingCommit> TransformTool::commit() {
  auto v = commits();
  if (v.empty()) return std::nullopt;
  return std::move(v.front());
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

std::array<std::array<float, 2>, 4> TransformTool::computeOuterCorners() const {
  const float bboxCx = outerOriginX_ + outerW_ * 0.5f;
  const float bboxCy = outerOriginY_ + outerH_ * 0.5f;
  const Affine2D m =
      buildDocToDoc(centerX_, centerY_, pivotX_, pivotY_, scaleX_, scaleY_,
                     angle_, bboxCx, bboxCy);
  std::array<std::array<float, 2>, 4> c;
  const float fX0 = static_cast<float>(outerOriginX_);
  const float fY0 = static_cast<float>(outerOriginY_);
  const float fX1 = static_cast<float>(outerOriginX_ + outerW_);
  const float fY1 = static_cast<float>(outerOriginY_ + outerH_);
  const float pts[4][2] = {{fX0, fY0}, {fX1, fY0}, {fX1, fY1}, {fX0, fY1}};
  for (int i = 0; i < 4; ++i) {
    m.mapPoint(pts[i][0], pts[i][1], c[i][0], c[i][1]);
  }
  return c;
}

bool TransformTool::pointInQuad(
    float x, float y, const std::array<std::array<float, 2>, 4>& q) const {
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
  const auto corners = computeOuterCorners();
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

void TransformTool::rebuildScratchFor(Source& s) const {
  // Apply the doc→doc transform to the source's 4 doc-coord corners; AABB
  // of the transformed corners gives the new doc rect for this source.
  const float bboxCx = outerOriginX_ + outerW_ * 0.5f;
  const float bboxCy = outerOriginY_ + outerH_ * 0.5f;
  const Affine2D docToDoc =
      buildDocToDoc(centerX_, centerY_, pivotX_, pivotY_, scaleX_, scaleY_,
                     angle_, bboxCx, bboxCy);
  const float fX0 = static_cast<float>(s.srcOriginX);
  const float fY0 = static_cast<float>(s.srcOriginY);
  const float fX1 = static_cast<float>(s.srcOriginX + s.srcW);
  const float fY1 = static_cast<float>(s.srcOriginY + s.srcH);
  const float pts[4][2] = {{fX0, fY0}, {fX1, fY0}, {fX1, fY1}, {fX0, fY1}};
  std::array<std::array<float, 2>, 4> tc;
  for (int i = 0; i < 4; ++i) {
    docToDoc.mapPoint(pts[i][0], pts[i][1], tc[i][0], tc[i][1]);
  }
  float minX = tc[0][0], maxX = tc[0][0];
  float minY = tc[0][1], maxY = tc[0][1];
  for (int i = 1; i < 4; ++i) {
    minX = std::min(minX, tc[i][0]);
    maxX = std::max(maxX, tc[i][0]);
    minY = std::min(minY, tc[i][1]);
    maxY = std::max(maxY, tc[i][1]);
  }
  const int ox = static_cast<int>(std::floor(minX));
  const int oy = static_cast<int>(std::floor(minY));
  const int w = std::max(1, static_cast<int>(std::ceil(maxX)) - ox);
  const int h = std::max(1, static_cast<int>(std::ceil(maxY)) - oy);

  s.scratchOriginX = ox;
  s.scratchOriginY = oy;
  s.scratch = TuxImage(w, h);

  // dst-local → src-local mapping for resampleBilinear:
  //   dst-local + scratchOrigin → dst-doc
  //   inverse(docToDoc)         → src-doc
  //   - srcOrigin               → src-local
  const Affine2D dstLocalToSrc =
      Affine2D::translation(static_cast<float>(ox), static_cast<float>(oy))
          .then(docToDoc.inverse())
          .then(Affine2D::translation(-static_cast<float>(s.srcOriginX),
                                       -static_cast<float>(s.srcOriginY)));
  resampleBilinear(s.src, s.scratch, dstLocalToSrc);
}

void TransformTool::rebuildScratch() {
  if (!active_) return;
  for (auto& s : sources_) rebuildScratchFor(s);
}

void TransformTool::markWholeDocDirty(const Document& doc) {
  dirty_ = {0, 0, doc.width(), doc.height()};
}

TransformTool::Overlay TransformTool::overlay() const {
  Overlay o;
  o.active = active_;
  if (active_ && !sources_.empty()) {
    o.overrides.reserve(sources_.size());
    for (const auto& s : sources_) {
      LayerOverride ov;
      ov.layerId = s.layerId;
      ov.image = &s.scratch;
      ov.originX = s.scratchOriginX;
      ov.originY = s.scratchOriginY;
      o.overrides.push_back(ov);
    }
    // Legacy: first source's override mirrored into `o.layer`.
    o.layer = o.overrides.front();
    o.corners = computeOuterCorners();
    o.pivot = {pivotX_, pivotY_};
  }
  return o;
}

void TransformTool::press(Document& doc, float x, float y, MouseButton btn) {
  if (!active_ || btn != MouseButton::Left) return;

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
    const auto q = computeOuterCorners();
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
  // pre-rotation frame relative to the pivot. The corner is on the OUTER
  // bbox (bbox-union of all sources). At drag start, the corner's
  // un-rotated, un-translated, pre-scale position relative to the pivot
  // is `(cornerX_initialDoc - pivotX, cornerY_initialDoc - pivotY)` in
  // the bbox-center frame.
  if (mode == DragMode::Scale && corner >= 0) {
    const float cornerInitX[4] = {static_cast<float>(outerOriginX_),
                                   static_cast<float>(outerOriginX_ + outerW_),
                                   static_cast<float>(outerOriginX_ + outerW_),
                                   static_cast<float>(outerOriginX_)};
    const float cornerInitY[4] = {static_cast<float>(outerOriginY_),
                                   static_cast<float>(outerOriginY_),
                                   static_cast<float>(outerOriginY_ + outerH_),
                                   static_cast<float>(outerOriginY_ + outerH_)};
    // The drag-start pivot is in doc coords; the corner's "local" coords
    // in the pre-rotation frame are `(corner - bboxCenter) + (bboxCenter
    // - pivot) = corner - pivot`. (The buildDocToDoc decomposition above
    // confirms this — at scale=1, angle=0, the doc→doc maps `corner` to
    // `corner + (centerX-bboxCx, centerY-bboxCy)`, so the local coord
    // relative to the pivot is `corner - pivot`.)
    dragStartLocalX_ = cornerInitX[corner] - dragStartPivotX_;
    dragStartLocalY_ = cornerInitY[corner] - dragStartPivotY_;
  } else {
    dragStartLocalX_ = 0.f;
    dragStartLocalY_ = 0.f;
  }

  markWholeDocDirty(doc);
}

void TransformTool::move(Document& /*doc*/, float x, float y) {
  if (!active_ || !dragging_) return;
  const float dx = x - dragStartX_;
  const float dy = y - dragStartY_;
  const bool shift = (modifiers_ & Mod::Shift) != 0;

  switch (dragMode_) {
    case DragMode::Translate: {
      centerX_ = dragStartCenterX_ + dx;
      centerY_ = dragStartCenterY_ + dy;
      pivotX_ = dragStartPivotX_ + dx;
      pivotY_ = dragStartPivotY_ + dy;
      break;
    }
    case DragMode::Rotate: {
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
        newSy = std::copysign(std::fabs(newSx), newSy);
      }
      scaleX_ = newSx;
      scaleY_ = newSy;
      break;
    }
    case DragMode::Pivot: {
      pivotX_ = dragStartPivotX_ + dx;
      pivotY_ = dragStartPivotY_ + dy;
      break;
    }
    case DragMode::None:
      break;
  }
  rebuildScratch();
  dirty_ = {0, 0, docW_, docH_};
}

void TransformTool::release(Document&, float, float, MouseButton btn) {
  if (!active_ || btn != MouseButton::Left) return;
  dragging_ = false;
  dragMode_ = DragMode::None;
  dragCorner_ = -1;
}

}  // namespace tuxels
