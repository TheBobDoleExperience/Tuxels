#include <memory>

#include "core/Document.h"
#include "layers/BrightnessContrast.h"
#include "layers/CloneLayer.h"
#include "layers/CurvesAdjustment.h"
#include "layers/GroupLayer.h"
#include "layers/HueSaturation.h"
#include "layers/LayerMask.h"
#include "layers/LevelsAdjustment.h"
#include "layers/PixelLayer.h"
#include "test_harness.h"

using namespace tuxels;

namespace {

PixelLayer* addPixel(Document& doc, GroupLayer* parent, const std::string& name,
                     int w = 8, int h = 8) {
  auto pl = std::make_unique<PixelLayer>(w, h);
  pl->name = name;
  pl->id = doc.nextLayerId();
  PixelLayer* raw = pl.get();
  if (parent) parent->children.push_back(std::move(pl));
  else doc.tree().add(std::move(pl));
  return raw;
}

}  // namespace

TEST(clone_pixel_layer_copies_image_and_props) {
  Document doc(64, 64);
  auto* a = addPixel(doc, nullptr, "A", 16, 16);
  a->image.fill(Rgba32F{0.5f, 0.f, 0.f, 1.f});
  a->visible = false;
  a->opacity = 0.7f;
  a->originX = 12;
  a->originY = 34;
  a->blend = BlendMode::Multiply;
  a->clipToBelow = true;
  a->mask = std::make_unique<LayerMask>(16, 16);
  a->mask->image.fill(Rgba32F{0.5f, 0.5f, 0.5f, 1.f});
  a->mask->enabled = false;

  auto out = cloneLayer(*a, doc);
  CHECK(out != nullptr);
  CHECK(out->id != a->id);
  CHECK(out->id != 0);
  CHECK(out->name == std::string("A copy"));
  CHECK(out->visible == false);
  CHECK_NEAR(out->opacity, 0.7f, 1e-6);
  CHECK_EQ(out->originX, 12);
  CHECK_EQ(out->originY, 34);
  CHECK(out->blend == BlendMode::Multiply);
  CHECK(out->clipToBelow == true);
  CHECK(out->mask != nullptr);
  CHECK(out->mask->enabled == false);

  // Image deep-copy via tile-COW: pixel reads return the same color.
  auto* outPx = dynamic_cast<PixelLayer*>(out.get());
  CHECK(outPx != nullptr);
  Rgba32F p = outPx->image.getPixel(0, 0);
  CHECK_NEAR(p.r, 0.5f, 1e-6);

  // Edits on the clone don't perturb the source (tile-COW).
  outPx->image.setPixel(0, 0, Rgba32F{1.f, 1.f, 1.f, 1.f});
  Rgba32F srcPixel = a->image.getPixel(0, 0);
  CHECK_NEAR(srcPixel.r, 0.5f, 1e-6);
}

TEST(clone_group_layer_recursively_clones_children) {
  Document doc(64, 64);
  auto g = std::make_unique<GroupLayer>();
  g->id = doc.nextLayerId();
  g->name = "G";
  g->isExpanded = false;
  g->blend = BlendMode::Normal;  // isolated group
  g->opacity = 0.8f;
  GroupLayer* gPtr = g.get();
  doc.tree().add(std::move(g));
  auto* a = addPixel(doc, gPtr, "A");
  auto* b = addPixel(doc, gPtr, "B");
  // Nested: another group with one child.
  auto inner = std::make_unique<GroupLayer>();
  inner->id = doc.nextLayerId();
  inner->name = "Inner";
  GroupLayer* innerPtr = inner.get();
  gPtr->children.push_back(std::move(inner));
  auto* c = addPixel(doc, innerPtr, "C");

  auto out = cloneLayer(*gPtr, doc);
  CHECK(out != nullptr);
  auto* outG = dynamic_cast<GroupLayer*>(out.get());
  CHECK(outG != nullptr);
  CHECK(outG->id != gPtr->id);
  CHECK(outG->name == std::string("G copy"));
  CHECK(outG->isExpanded == false);
  CHECK_NEAR(outG->opacity, 0.8f, 1e-6);
  CHECK_EQ(static_cast<int>(outG->children.size()), 3);
  // Children get fresh ids (no collision with originals).
  CHECK(outG->children[0]->id != a->id);
  CHECK(outG->children[1]->id != b->id);
  CHECK(outG->children[2]->id != innerPtr->id);
  auto* outInner = dynamic_cast<GroupLayer*>(outG->children[2].get());
  CHECK(outInner != nullptr);
  CHECK_EQ(static_cast<int>(outInner->children.size()), 1);
  CHECK(outInner->children[0]->id != c->id);
  CHECK(outInner->children[0]->name == std::string("C copy"));
}

TEST(clone_levels_layer_replicates_params) {
  Document doc(64, 64);
  auto layer = std::make_unique<LevelsAdjustment>();
  layer->id = doc.nextLayerId();
  layer->name = "Lv";
  // Bend the composite channel away from identity.
  LevelsParams p;
  p.inBlack = 0.1f;
  p.inWhite = 0.9f;
  p.gamma = 0.5f;
  p.outBlack = 0.05f;
  p.outWhite = 0.95f;
  layer->setParams(LevelsChannel::Composite, p);

  auto out = cloneLayer(*layer, doc);
  CHECK(out != nullptr);
  auto* outLv = dynamic_cast<LevelsAdjustment*>(out.get());
  CHECK(outLv != nullptr);
  CHECK(outLv->id != layer->id);
  CHECK(outLv->name == std::string("Lv copy"));
  const auto& cp = outLv->params(LevelsChannel::Composite);
  CHECK_NEAR(cp.inBlack, 0.1f, 1e-6);
  CHECK_NEAR(cp.gamma, 0.5f, 1e-6);
  CHECK_NEAR(cp.outWhite, 0.95f, 1e-6);
}

TEST(clone_huesat_layer_replicates_params) {
  Document doc(64, 64);
  auto layer = std::make_unique<HueSaturation>();
  layer->id = doc.nextLayerId();
  layer->name = "HS";
  HueSaturationParams p;
  p.hueShift = 60.f;
  p.saturation = 0.5f;
  p.lightness = -0.2f;
  layer->setParams(p);

  auto out = cloneLayer(*layer, doc);
  CHECK(out != nullptr);
  auto* outHs = dynamic_cast<HueSaturation*>(out.get());
  CHECK(outHs != nullptr);
  CHECK_NEAR(outHs->params().hueShift, 60.f, 1e-6);
  CHECK_NEAR(outHs->params().saturation, 0.5f, 1e-6);
  CHECK_NEAR(outHs->params().lightness, -0.2f, 1e-6);
}

TEST(clone_brightness_contrast_layer_replicates_params) {
  Document doc(64, 64);
  auto layer = std::make_unique<BrightnessContrast>();
  layer->id = doc.nextLayerId();
  BrightnessContrastParams p;
  p.brightness = 0.3f;
  p.contrast = -0.4f;
  layer->setParams(p);

  auto out = cloneLayer(*layer, doc);
  CHECK(out != nullptr);
  auto* outBc = dynamic_cast<BrightnessContrast*>(out.get());
  CHECK(outBc != nullptr);
  CHECK_NEAR(outBc->params().brightness, 0.3f, 1e-6);
  CHECK_NEAR(outBc->params().contrast, -0.4f, 1e-6);
}

TEST(clone_curves_layer_replicates_points) {
  Document doc(64, 64);
  auto layer = std::make_unique<CurvesAdjustment>();
  layer->id = doc.nextLayerId();
  CurvesAdjustment::PointsArray pts{};
  pts[0] = {{0.f, 0.f}, {0.5f, 0.7f}, {1.f, 1.f}};
  pts[1] = {{0.f, 0.f}, {1.f, 1.f}};
  pts[2] = {{0.f, 0.f}, {1.f, 1.f}};
  pts[3] = {{0.f, 0.f}, {1.f, 1.f}};
  layer->setAllPoints(pts);

  auto out = cloneLayer(*layer, doc);
  CHECK(out != nullptr);
  auto* outCv = dynamic_cast<CurvesAdjustment*>(out.get());
  CHECK(outCv != nullptr);
  CHECK_EQ(static_cast<int>(outCv->allPoints()[0].size()), 3);
  CHECK_NEAR(outCv->allPoints()[0][1].y, 0.7f, 1e-6);
}

int main() { return tuxels::testing::run(); }
