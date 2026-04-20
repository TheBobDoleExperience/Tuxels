#include "tools/BucketTool.h"

#include <cmath>

#include "core/Document.h"
#include "layers/LayerMask.h"
#include "layers/PixelLayer.h"

namespace tuxels {

void BucketTool::press(Document& doc, float x, float y, MouseButton btn) {
  if (btn != MouseButton::Left) return;
  auto* px = dynamic_cast<PixelLayer*>(doc.activeLayer());
  if (!px) return;

  TuxImage* target = &px->image;
  if (doc.paintTarget() == PaintTarget::Mask && px->mask) {
    target = &px->mask->image;
  }

  const int sx = static_cast<int>(std::floor(x));
  const int sy = static_cast<int>(std::floor(y));

  target->beginRecord();
  FloodFillResult r =
      floodFill(*target, sx, sy, color_, opts_, doc.selection());
  TuxImage::Recorded rec = target->stopRecord();

  dirty_ = r.bounds;
  if (r.changed) {
    last_ = LastFill{px, target, r.bounds, std::move(rec)};
  }
}

}  // namespace tuxels
