#include "tools/CropTool.h"

#include <algorithm>
#include <cmath>

#include "core/Document.h"

namespace tuxels {

Rect CropTool::computeRect() const {
  const int x0 = std::min(startX_, curX_);
  const int y0 = std::min(startY_, curY_);
  const int x1 = std::max(startX_, curX_);
  const int y1 = std::max(startY_, curY_);
  return Rect{x0, y0, (x1 - x0) + 1, (y1 - y0) + 1};
}

void CropTool::press(Document& doc, float x, float y, MouseButton btn) {
  if (btn != MouseButton::Left) return;
  dragging_ = true;
  startX_ = curX_ = static_cast<int>(std::floor(x));
  startY_ = curY_ = static_cast<int>(std::floor(y));
  docW_ = doc.width();
  docH_ = doc.height();
}

void CropTool::move(Document& /*doc*/, float x, float y) {
  if (!dragging_) return;
  curX_ = static_cast<int>(std::floor(x));
  curY_ = static_cast<int>(std::floor(y));
}

void CropTool::release(Document& /*doc*/, float x, float y, MouseButton btn) {
  if (btn != MouseButton::Left || !dragging_) return;
  curX_ = static_cast<int>(std::floor(x));
  curY_ = static_cast<int>(std::floor(y));
  dragging_ = false;

  Rect r = computeRect();
  const int x0 = std::max(r.x, 0);
  const int y0 = std::max(r.y, 0);
  const int x1 = std::min(r.right(), docW_);
  const int y1 = std::min(r.bottom(), docH_);
  if (x1 - x0 < 2 || y1 - y0 < 2) return;  // ignore degenerate drags

  pending_ = PendingCrop{Rect{x0, y0, x1 - x0, y1 - y0}};
}

}  // namespace tuxels
