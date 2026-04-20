#include "tools/MoveTool.h"

#include <algorithm>
#include <cmath>

#include "core/Document.h"
#include "layers/PixelLayer.h"

namespace tuxels {

namespace {

Rect docRectOfLayer(int ox, int oy, int w, int h, int docW, int docH) {
  const int x0 = std::max(0, ox);
  const int y0 = std::max(0, oy);
  const int x1 = std::min(docW, ox + w);
  const int y1 = std::min(docH, oy + h);
  if (x1 <= x0 || y1 <= y0) return {};
  return {x0, y0, x1 - x0, y1 - y0};
}

Rect unionRect(Rect a, Rect b) {
  if (a.isEmpty()) return b;
  if (b.isEmpty()) return a;
  const int x0 = std::min(a.x, b.x);
  const int y0 = std::min(a.y, b.y);
  const int x1 = std::max(a.right(), b.right());
  const int y1 = std::max(a.bottom(), b.bottom());
  return {x0, y0, x1 - x0, y1 - y0};
}

}  // namespace

void MoveTool::press(Document& doc, float x, float y, MouseButton btn) {
  if (btn != MouseButton::Left) return;
  LayerBase* base = doc.activeLayer();
  if (!base) return;
  auto* px = dynamic_cast<PixelLayer*>(base);
  if (!px) return;  // Only pixel layers carry backing imagery to move.

  dragging_ = true;
  layerId_ = base->id;
  beforeX_ = base->originX;
  beforeY_ = base->originY;
  startMouseX_ = static_cast<int>(std::floor(x));
  startMouseY_ = static_cast<int>(std::floor(y));
  layerW_ = px->image.width();
  layerH_ = px->image.height();
  docW_ = doc.width();
  docH_ = doc.height();
  pending_.reset();
  dirty_ = {};
}

void MoveTool::move(Document& doc, float x, float y) {
  if (!dragging_) return;
  LayerBase* base = doc.activeLayer();
  // Bail if the active layer was swapped mid-drag — we'd otherwise move
  // the wrong layer.
  if (!base || base->id != layerId_) {
    dragging_ = false;
    return;
  }
  const int curX = static_cast<int>(std::floor(x));
  const int curY = static_cast<int>(std::floor(y));
  const int newX = beforeX_ + (curX - startMouseX_);
  const int newY = beforeY_ + (curY - startMouseY_);
  const int oldOx = base->originX;
  const int oldOy = base->originY;
  if (newX == oldOx && newY == oldOy) return;

  base->originX = newX;
  base->originY = newY;

  // Union the visible-in-doc regions of the old and new positions so the
  // canvas recomposites exactly the pixels that changed. Much cheaper than
  // a full recomposite on large docs.
  const Rect beforeRect =
      docRectOfLayer(oldOx, oldOy, layerW_, layerH_, docW_, docH_);
  const Rect afterRect =
      docRectOfLayer(newX, newY, layerW_, layerH_, docW_, docH_);
  dirty_ = unionRect(dirty_, unionRect(beforeRect, afterRect));
}

void MoveTool::release(Document& doc, float x, float y, MouseButton btn) {
  if (btn != MouseButton::Left) return;
  if (!dragging_) return;
  // Fold any final cursor delta in, then latch the commit.
  move(doc, x, y);
  dragging_ = false;
  LayerBase* base = doc.activeLayer();
  if (!base || base->id != layerId_) return;
  if (base->originX == beforeX_ && base->originY == beforeY_) return;
  PendingMove p;
  p.layerId = layerId_;
  p.beforeX = beforeX_;
  p.beforeY = beforeY_;
  p.afterX = base->originX;
  p.afterY = base->originY;
  pending_ = p;
}

}  // namespace tuxels
