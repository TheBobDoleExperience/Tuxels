#include "layers/CloneLayer.h"

#include "core/Document.h"
#include "layers/BrightnessContrast.h"
#include "layers/CurvesAdjustment.h"
#include "layers/GroupLayer.h"
#include "layers/HueSaturation.h"
#include "layers/LayerMask.h"
#include "layers/LevelsAdjustment.h"
#include "layers/PixelLayer.h"

namespace tuxels {

namespace {

// Deep-copy a TuxImage by walking each present tile and cloning its
// contents — TuxImage's default copy ctor only shares the per-tile
// shared_ptrs, which would mean writes on the clone perturb the source
// outside of a beginRecord/stopRecord window. For Duplicate Layer we
// need fully independent storage so the user can paint on either copy
// without affecting the other.
TuxImage deepCopyImage(const TuxImage& src) {
  TuxImage out(src.width(), src.height());
  for (const auto& kv : src.tiles()) {
    if (kv.second) out.tiles().set(kv.first, kv.second->clone());
  }
  return out;
}

// Copy LayerBase's mutable common fields from `src` onto `dst`. Caller is
// responsible for assigning `dst->id` (clone always picks a fresh id).
// `mask` is unique_ptr — we deep-copy the LayerMask (image + enabled).
void copyCommonBaseFields(LayerBase& dst, const LayerBase& src) {
  dst.name = src.name + " copy";
  dst.visible = src.visible;
  dst.opacity = src.opacity;
  dst.blend = src.blend;
  dst.originX = src.originX;
  dst.originY = src.originY;
  dst.clipToBelow = src.clipToBelow;
  if (src.mask) {
    auto m = std::make_unique<LayerMask>();
    m->image = deepCopyImage(src.mask->image);
    m->enabled = src.mask->enabled;
    dst.mask = std::move(m);
  }
}

}  // namespace

std::unique_ptr<LayerBase> cloneLayer(const LayerBase& src, Document& doc) {
  if (auto* px = dynamic_cast<const PixelLayer*>(&src)) {
    auto out = std::make_unique<PixelLayer>();
    out->image = deepCopyImage(px->image);
    out->id = doc.nextLayerId();
    copyCommonBaseFields(*out, src);
    return out;
  }
  if (auto* g = dynamic_cast<const GroupLayer*>(&src)) {
    auto out = std::make_unique<GroupLayer>();
    out->id = doc.nextLayerId();
    out->isExpanded = g->isExpanded;
    copyCommonBaseFields(*out, src);
    out->children.reserve(g->children.size());
    for (const auto& c : g->children) {
      if (c) out->children.push_back(cloneLayer(*c, doc));
    }
    return out;
  }
  // Adjustment subclasses: LayerBase carries a unique_ptr<LayerMask> so the
  // implicit copy ctor is deleted. Construct fresh + replay params via the
  // public bulk setters; copyCommonBaseFields handles the mask deep-copy.
  if (auto* lv = dynamic_cast<const LevelsAdjustment*>(&src)) {
    auto out = std::make_unique<LevelsAdjustment>();
    out->setAllParams(lv->allParams());
    out->id = doc.nextLayerId();
    copyCommonBaseFields(*out, src);
    return out;
  }
  if (auto* cv = dynamic_cast<const CurvesAdjustment*>(&src)) {
    auto out = std::make_unique<CurvesAdjustment>();
    out->setAllPoints(cv->allPoints());
    out->id = doc.nextLayerId();
    copyCommonBaseFields(*out, src);
    return out;
  }
  if (auto* hs = dynamic_cast<const HueSaturation*>(&src)) {
    auto out = std::make_unique<HueSaturation>();
    out->setParams(hs->params());
    out->id = doc.nextLayerId();
    copyCommonBaseFields(*out, src);
    return out;
  }
  if (auto* bc = dynamic_cast<const BrightnessContrast*>(&src)) {
    auto out = std::make_unique<BrightnessContrast>();
    out->setParams(bc->params());
    out->id = doc.nextLayerId();
    copyCommonBaseFields(*out, src);
    return out;
  }
  return nullptr;
}

}  // namespace tuxels
