#include "brush/BrushEngine.h"

#include <algorithm>
#include <cmath>

#include "core/SelectionMask.h"

namespace tuxels {

void BrushEngine::growBounds(int x0, int y0, int x1, int y1) {
  if (x1 <= x0 || y1 <= y0) return;
  auto grow = [&](Rect& r) {
    if (r.isEmpty()) {
      r = {x0, y0, x1 - x0, y1 - y0};
      return;
    }
    const int nx = std::min(r.x, x0);
    const int ny = std::min(r.y, y0);
    const int nr = std::max(r.right(), x1);
    const int nb = std::max(r.bottom(), y1);
    r = {nx, ny, nr - nx, nb - ny};
  };
  grow(bounds_);
  grow(incremental_);
}

void BrushEngine::applyStamp(float cx, float cy) {
  const int D = brush_.diameter();
  const float radius = D * 0.5f;
  const int x0 = static_cast<int>(std::floor(cx - radius));
  const int y0 = static_cast<int>(std::floor(cy - radius));

  const auto& p = brush_.params();
  const Rgba32F col = p.color;
  const float op = p.opacity;
  const float flow = p.flow;

  const int xMin = std::max(0, x0);
  const int yMin = std::max(0, y0);
  const int xMax = std::min(target_.width(), x0 + D);
  const int yMax = std::min(target_.height(), y0 + D);
  if (xMax <= xMin || yMax <= yMin) return;

  for (int y = yMin; y < yMax; ++y) {
    for (int x = xMin; x < xMax; ++x) {
      const float k = brush_.kernel(x - x0, y - y0);
      if (k <= 0.f) continue;
      const float s = selection_ ? selection_->sample(x, y) : 1.f;
      if (s <= 0.f) continue;
      const float a = k * op * flow * col.a * s;
      if (a <= 0.f) continue;
      const Rgba32F surface = target_.getPixel(x, y);
      const float inv = 1.f - a;
      Rgba32F out;
      out.r = a * col.r + inv * surface.r;
      out.g = a * col.g + inv * surface.g;
      out.b = a * col.b + inv * surface.b;
      out.a = a + inv * surface.a;
      target_.setPixel(x, y, out);
    }
  }
  growBounds(xMin, yMin, xMax, yMax);
}

void BrushEngine::beginStroke(float x, float y) {
  active_ = true;
  lastX_ = x;
  lastY_ = y;
  distAccum_ = 0.f;
  applyStamp(x, y);
}

void BrushEngine::continueStroke(float x, float y) {
  if (!active_) {
    beginStroke(x, y);
    return;
  }
  const float spacing = static_cast<float>(brush_.spacingPx());
  const float dx = x - lastX_;
  const float dy = y - lastY_;
  const float dist = std::sqrt(dx * dx + dy * dy);
  if (dist <= 0.f) return;

  const float total = distAccum_ + dist;
  const int nStamps = static_cast<int>(total / spacing);
  for (int i = 1; i <= nStamps; ++i) {
    const float wanted = i * spacing - distAccum_;
    const float tt = wanted / dist;
    applyStamp(lastX_ + tt * dx, lastY_ + tt * dy);
  }
  distAccum_ = total - nStamps * spacing;
  lastX_ = x;
  lastY_ = y;
}

void BrushEngine::endStroke() {
  active_ = false;
  distAccum_ = 0.f;
}

}  // namespace tuxels
