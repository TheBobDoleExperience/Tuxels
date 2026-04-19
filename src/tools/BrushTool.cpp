#include "tools/BrushTool.h"

#include "brush/BrushEngine.h"
#include "core/Document.h"
#include "layers/LayerMask.h"
#include "layers/PixelLayer.h"

namespace tuxels {

BrushTool::BrushTool() = default;
BrushTool::~BrushTool() = default;

void BrushTool::press(Document& doc, float x, float y, MouseButton btn) {
  if (btn != MouseButton::Left) return;
  auto* px = dynamic_cast<PixelLayer*>(doc.activeLayer());
  if (!px) return;
  active_ = px;
  activeTarget_ = &px->image;
  if (doc.paintTarget() == PaintTarget::Mask && px->mask) {
    activeTarget_ = &px->mask->image;
  }
  activeTarget_->beginRecord();
  engine_ = std::make_unique<BrushEngine>(brush_, *activeTarget_);
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

}  // namespace tuxels
