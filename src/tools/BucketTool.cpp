#include "tools/BucketTool.h"

#include <cmath>

#include "core/Document.h"
#include "layers/LayerBase.h"
#include "layers/LayerMask.h"
#include "layers/PixelLayer.h"

namespace tuxels {

void BucketTool::press(Document& doc, float x, float y, MouseButton btn) {
  if (btn != MouseButton::Left) return;
  LayerBase* base = doc.activeLayer();
  if (!base) return;

  TuxImage* target = nullptr;
  if (doc.paintTarget() == PaintTarget::Mask && base->mask) {
    target = &base->mask->image;
  } else if (auto* px = dynamic_cast<PixelLayer*>(base)) {
    target = &px->image;
  }
  if (!target) return;

  const int sx = static_cast<int>(std::floor(x));
  const int sy = static_cast<int>(std::floor(y));

  target->beginRecord();
  FloodFillResult r =
      floodFill(*target, sx, sy, color_, opts_, doc.selection());
  TuxImage::Recorded rec = target->stopRecord();

  dirty_ = r.bounds;
  if (r.changed) {
    last_ = LastFill{base, target, r.bounds, std::move(rec)};
  }
}

}  // namespace tuxels
