#include "tools/BrushTool.h"

#include "brush/BrushEngine.h"
#include "core/Document.h"
#include "layers/PixelLayer.h"

namespace tuxels {

BrushTool::BrushTool() = default;
BrushTool::~BrushTool() = default;

void BrushTool::press(Document& doc, float x, float y, MouseButton btn) {
  if (btn != MouseButton::Left) return;
  auto* px = dynamic_cast<PixelLayer*>(doc.activeLayer());
  if (!px) return;
  active_ = px;
  active_->image.beginRecord();
  engine_ = std::make_unique<BrushEngine>(brush_, active_->image);
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
  TuxImage::Recorded rec = active_->image.stopRecord();
  last_ = {active_, engine_->strokeBounds(), std::move(rec)};
  engine_.reset();
  active_ = nullptr;
}

}  // namespace tuxels
