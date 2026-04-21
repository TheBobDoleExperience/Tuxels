#include "tools/BrushTool.h"

#include "brush/BrushEngine.h"
#include "core/Document.h"
#include "layers/LayerBase.h"
#include "layers/LayerMask.h"
#include "layers/PixelLayer.h"

namespace tuxels {

BrushTool::BrushTool() = default;
BrushTool::~BrushTool() = default;

void BrushTool::press(Document& doc, float x, float y, MouseButton btn) {
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
  active_ = base;
  activeTarget_ = target;
  activeTarget_->beginRecord();
  engine_ = std::make_unique<BrushEngine>(brush_, *activeTarget_, doc.selection());
  engine_->beginStroke(x, y);
}

void BrushTool::move(Document& /*doc*/, float x, float y) {
  if (!engine_) return;
  engine_->continueStroke(x, y);
}

void BrushTool::release(Document& /*doc*/, float x, float y, MouseButton btn) {
  if (btn != MouseButton::Left || !engine_) return;
  engine_->continueStroke(x, y);
  engine_->endStroke();
  TuxImage::Recorded rec = activeTarget_->stopRecord();
  last_ = {active_, activeTarget_, engine_->strokeBounds(), std::move(rec)};
  engine_.reset();
  active_ = nullptr;
  activeTarget_ = nullptr;
}

Rect BrushTool::takeDirtyRect() {
  if (!engine_) return {};
  return engine_->takeIncrementalBounds();
}

}  // namespace tuxels
