#include "ui/PropertiesPaneHueSat.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QSlider>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace tuxels {

namespace {

constexpr int kHueMin = -180;
constexpr int kHueMax = 180;
constexpr int kPctMin = -100;
constexpr int kPctMax = 100;

}  // namespace

PropertiesPaneHueSat::PropertiesPaneHueSat(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(6);

  auto* form = new QFormLayout();
  form->setContentsMargins(0, 0, 0, 0);
  form->setSpacing(4);

  auto makeRow = [&](QSlider*& slider, QDoubleSpinBox*& spin, int lo, int hi) {
    slider = new QSlider(Qt::Horizontal, this);
    slider->setRange(lo, hi);
    spin = new QDoubleSpinBox(this);
    spin->setRange(lo, hi);
    spin->setDecimals(0);
    spin->setSingleStep(1.0);
    auto* row = new QHBoxLayout();
    row->addWidget(slider, 1);
    row->addWidget(spin);
    return row;
  };

  form->addRow(tr("Hue"), makeRow(hueSlider_, hueSpin_, kHueMin, kHueMax));
  form->addRow(tr("Saturation"),
               makeRow(satSlider_, satSpin_, kPctMin, kPctMax));
  form->addRow(tr("Lightness"),
               makeRow(lightSlider_, lightSpin_, kPctMin, kPctMax));
  layout->addLayout(form);
  layout->addStretch(1);

  for (auto* s : {hueSlider_, satSlider_, lightSlider_}) {
    connect(s, &QSlider::sliderPressed, this,
            &PropertiesPaneHueSat::onSliderPressed);
    connect(s, &QSlider::sliderReleased, this,
            &PropertiesPaneHueSat::onSliderReleased);
  }
  for (auto* sp : {hueSpin_, satSpin_, lightSpin_}) {
    connect(sp, &QDoubleSpinBox::editingFinished, this,
            &PropertiesPaneHueSat::onSpinEditingFinished);
  }
  connect(hueSlider_, &QSlider::valueChanged, this,
          &PropertiesPaneHueSat::onHueChanged);
  connect(hueSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &PropertiesPaneHueSat::onHueChanged);
  connect(satSlider_, &QSlider::valueChanged, this,
          &PropertiesPaneHueSat::onSaturationChanged);
  connect(satSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &PropertiesPaneHueSat::onSaturationChanged);
  connect(lightSlider_, &QSlider::valueChanged, this,
          &PropertiesPaneHueSat::onLightnessChanged);
  connect(lightSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &PropertiesPaneHueSat::onLightnessChanged);
}

void PropertiesPaneHueSat::bind(HueSaturation* layer) {
  layer_ = layer;
  if (!layer_) return;
  snapshotBefore();
  loadFromLayer();
}

void PropertiesPaneHueSat::unbind() { layer_ = nullptr; }

void PropertiesPaneHueSat::snapshotBefore() {
  if (!layer_) return;
  paramsBefore_ = layer_->params();
}

bool PropertiesPaneHueSat::paramsDifferFromBefore() const {
  if (!layer_) return false;
  const auto& cur = layer_->params();
  return cur.hueShift != paramsBefore_.hueShift ||
         cur.saturation != paramsBefore_.saturation ||
         cur.lightness != paramsBefore_.lightness;
}

void PropertiesPaneHueSat::loadFromLayer() {
  if (!layer_) return;
  loading_ = true;
  const auto& p = layer_->params();
  hueSpin_->setValue(p.hueShift);
  hueSlider_->setValue(static_cast<int>(std::round(p.hueShift)));
  satSpin_->setValue(p.saturation * 100.f);
  satSlider_->setValue(static_cast<int>(std::round(p.saturation * 100.f)));
  lightSpin_->setValue(p.lightness * 100.f);
  lightSlider_->setValue(static_cast<int>(std::round(p.lightness * 100.f)));
  loading_ = false;
}

void PropertiesPaneHueSat::pushParamsFromWidgets() {
  if (loading_ || !layer_) return;
  HueSaturationParams p;
  p.hueShift = static_cast<float>(hueSpin_->value());
  p.saturation = static_cast<float>(satSpin_->value()) / 100.f;
  p.lightness = static_cast<float>(lightSpin_->value()) / 100.f;
  layer_->setParams(p);
  emit previewChanged();
}

void PropertiesPaneHueSat::simulateSliderPressForTest() {
  onSliderPressed();
}
void PropertiesPaneHueSat::simulateSliderReleaseForTest() {
  onSliderReleased();
}

void PropertiesPaneHueSat::onSliderPressed() {
  if (!layer_) return;
  snapshotBefore();
}

void PropertiesPaneHueSat::onSliderReleased() {
  if (!layer_) return;
  if (paramsDifferFromBefore()) {
    emit commitRequested(layer_, paramsBefore_, layer_->params());
    snapshotBefore();
  }
}

void PropertiesPaneHueSat::onSpinEditingFinished() {
  if (!layer_) return;
  if (paramsDifferFromBefore()) {
    emit commitRequested(layer_, paramsBefore_, layer_->params());
    snapshotBefore();
  }
}

void PropertiesPaneHueSat::onHueChanged() {
  if (loading_) return;
  if (sender() == hueSlider_) {
    loading_ = true;
    hueSpin_->setValue(static_cast<double>(hueSlider_->value()));
    loading_ = false;
  } else if (sender() == hueSpin_) {
    loading_ = true;
    hueSlider_->setValue(static_cast<int>(std::round(hueSpin_->value())));
    loading_ = false;
  }
  pushParamsFromWidgets();
}

void PropertiesPaneHueSat::onSaturationChanged() {
  if (loading_) return;
  if (sender() == satSlider_) {
    loading_ = true;
    satSpin_->setValue(static_cast<double>(satSlider_->value()));
    loading_ = false;
  } else if (sender() == satSpin_) {
    loading_ = true;
    satSlider_->setValue(static_cast<int>(std::round(satSpin_->value())));
    loading_ = false;
  }
  pushParamsFromWidgets();
}

void PropertiesPaneHueSat::onLightnessChanged() {
  if (loading_) return;
  if (sender() == lightSlider_) {
    loading_ = true;
    lightSpin_->setValue(static_cast<double>(lightSlider_->value()));
    loading_ = false;
  } else if (sender() == lightSpin_) {
    loading_ = true;
    lightSlider_->setValue(static_cast<int>(std::round(lightSpin_->value())));
    loading_ = false;
  }
  pushParamsFromWidgets();
}

}  // namespace tuxels
