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

  // Build the drag set: every PixelLayer in the multi-selection, falling
  // back to just the active layer when no multi-selection is active.
  // Non-pixel layers (groups, adjustments) are skipped — they have no
  // origin we can shift.
  dragLayers_.clear();
  pending_.clear();
  dirty_ = {};

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
    DragLayer d;
    d.id = l->id;
    d.beforeX = l->originX;
    d.beforeY = l->originY;
    d.layerW = px->image.width();
    d.layerH = px->image.height();
    dragLayers_.push_back(d);
  }
  if (dragLayers_.empty()) return;

  dragging_ = true;
  startMouseX_ = static_cast<int>(std::floor(x));
  startMouseY_ = static_cast<int>(std::floor(y));
  docW_ = doc.width();
  docH_ = doc.height();
}

void MoveTool::move(Document& doc, float x, float y) {
  if (!dragging_) return;
  const int curX = static_cast<int>(std::floor(x));
  const int curY = static_cast<int>(std::floor(y));
  const int dx = curX - startMouseX_;
  const int dy = curY - startMouseY_;

  for (const auto& d : dragLayers_) {
    LayerBase* base = doc.tree().findById(d.id);
    if (!base) continue;
    const int newX = d.beforeX + dx;
    const int newY = d.beforeY + dy;
    const int oldOx = base->originX;
    const int oldOy = base->originY;
    if (newX == oldOx && newY == oldOy) continue;

    base->originX = newX;
    base->originY = newY;

    const Rect beforeRect =
        docRectOfLayer(oldOx, oldOy, d.layerW, d.layerH, docW_, docH_);
    const Rect afterRect =
        docRectOfLayer(newX, newY, d.layerW, d.layerH, docW_, docH_);
    dirty_ = unionRect(dirty_, unionRect(beforeRect, afterRect));
  }
}

void MoveTool::release(Document& doc, float x, float y, MouseButton btn) {
  if (btn != MouseButton::Left) return;
  if (!dragging_) return;
  // Fold any final cursor delta in, then latch the commits.
  move(doc, x, y);
  dragging_ = false;

  for (const auto& d : dragLayers_) {
    LayerBase* base = doc.tree().findById(d.id);
    if (!base) continue;
    if (base->originX == d.beforeX && base->originY == d.beforeY) continue;
    PendingMove p;
    p.layerId = d.id;
    p.beforeX = d.beforeX;
    p.beforeY = d.beforeY;
    p.afterX = base->originX;
    p.afterY = base->originY;
    pending_.push_back(p);
  }
  dragLayers_.clear();
}

}  // namespace tuxels
