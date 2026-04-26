#include "ui/PropertiesPaneBrightnessContrast.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QSlider>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace tuxels {

namespace {

constexpr int kSliderMin = -100;
constexpr int kSliderMax = 100;

int paramToSlider(float v) {
  const float s = std::clamp(v * 100.f, static_cast<float>(kSliderMin),
                             static_cast<float>(kSliderMax));
  return static_cast<int>(std::round(s));
}

}  // namespace

PropertiesPaneBrightnessContrast::PropertiesPaneBrightnessContrast(
    QWidget* parent)
    : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(6);

  auto* form = new QFormLayout();
  form->setContentsMargins(0, 0, 0, 0);
  form->setSpacing(4);

  auto makeRow = [&](QSlider*& slider, QDoubleSpinBox*& spin) {
    slider = new QSlider(Qt::Horizontal, this);
    slider->setRange(kSliderMin, kSliderMax);
    spin = new QDoubleSpinBox(this);
    spin->setRange(kSliderMin, kSliderMax);
    spin->setDecimals(0);
    spin->setSingleStep(1.0);
    auto* row = new QHBoxLayout();
    row->addWidget(slider, 1);
    row->addWidget(spin);
    return row;
  };

  form->addRow(tr("Brightness"), makeRow(brightnessSlider_, brightnessSpin_));
  form->addRow(tr("Contrast"), makeRow(contrastSlider_, contrastSpin_));
  layout->addLayout(form);
  layout->addStretch(1);

  for (auto* s : {brightnessSlider_, contrastSlider_}) {
    connect(s, &QSlider::sliderPressed, this,
            &PropertiesPaneBrightnessContrast::onSliderPressed);
    connect(s, &QSlider::sliderReleased, this,
            &PropertiesPaneBrightnessContrast::onSliderReleased);
  }
  for (auto* sp : {brightnessSpin_, contrastSpin_}) {
    connect(sp, &QDoubleSpinBox::editingFinished, this,
            &PropertiesPaneBrightnessContrast::onSpinEditingFinished);
  }
  connect(brightnessSlider_, &QSlider::valueChanged, this,
          &PropertiesPaneBrightnessContrast::onBrightnessChanged);
  connect(brightnessSpin_,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &PropertiesPaneBrightnessContrast::onBrightnessChanged);
  connect(contrastSlider_, &QSlider::valueChanged, this,
          &PropertiesPaneBrightnessContrast::onContrastChanged);
  connect(contrastSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &PropertiesPaneBrightnessContrast::onContrastChanged);
}

void PropertiesPaneBrightnessContrast::bind(BrightnessContrast* layer) {
  layer_ = layer;
  if (!layer_) return;
  snapshotBefore();
  loadFromLayer();
}

void PropertiesPaneBrightnessContrast::unbind() { layer_ = nullptr; }

void PropertiesPaneBrightnessContrast::snapshotBefore() {
  if (!layer_) return;
  paramsBefore_ = layer_->params();
}

bool PropertiesPaneBrightnessContrast::paramsDifferFromBefore() const {
  if (!layer_) return false;
  const auto& cur = layer_->params();
  return cur.brightness != paramsBefore_.brightness ||
         cur.contrast != paramsBefore_.contrast;
}

void PropertiesPaneBrightnessContrast::loadFromLayer() {
  if (!layer_) return;
  loading_ = true;
  const auto& p = layer_->params();
  brightnessSpin_->setValue(p.brightness * 100.f);
  brightnessSlider_->setValue(paramToSlider(p.brightness));
  contrastSpin_->setValue(p.contrast * 100.f);
  contrastSlider_->setValue(paramToSlider(p.contrast));
  loading_ = false;
}

void PropertiesPaneBrightnessContrast::pushParamsFromWidgets() {
  if (loading_ || !layer_) return;
  BrightnessContrastParams p;
  p.brightness = static_cast<float>(brightnessSpin_->value()) / 100.f;
  p.contrast = static_cast<float>(contrastSpin_->value()) / 100.f;
  layer_->setParams(p);
  emit previewChanged();
}

void PropertiesPaneBrightnessContrast::simulateSliderPressForTest() {
  onSliderPressed();
}
void PropertiesPaneBrightnessContrast::simulateSliderReleaseForTest() {
  onSliderReleased();
}

void PropertiesPaneBrightnessContrast::onSliderPressed() {
  if (!layer_) return;
  snapshotBefore();
}

void PropertiesPaneBrightnessContrast::onSliderReleased() {
  if (!layer_) return;
  if (paramsDifferFromBefore()) {
    emit commitRequested(layer_, paramsBefore_, layer_->params());
    snapshotBefore();
  }
}

void PropertiesPaneBrightnessContrast::onSpinEditingFinished() {
  if (!layer_) return;
  if (paramsDifferFromBefore()) {
    emit commitRequested(layer_, paramsBefore_, layer_->params());
    snapshotBefore();
  }
}

void PropertiesPaneBrightnessContrast::onBrightnessChanged() {
  if (loading_) return;
  if (sender() == brightnessSlider_) {
    loading_ = true;
    brightnessSpin_->setValue(
        static_cast<double>(brightnessSlider_->value()));
    loading_ = false;
  } else if (sender() == brightnessSpin_) {
    loading_ = true;
    brightnessSlider_->setValue(
        paramToSlider(static_cast<float>(brightnessSpin_->value()) / 100.f));
    loading_ = false;
  }
  pushParamsFromWidgets();
}

void PropertiesPaneBrightnessContrast::onContrastChanged() {
  if (loading_) return;
  if (sender() == contrastSlider_) {
    loading_ = true;
    contrastSpin_->setValue(static_cast<double>(contrastSlider_->value()));
    loading_ = false;
  } else if (sender() == contrastSpin_) {
    loading_ = true;
    contrastSlider_->setValue(
        paramToSlider(static_cast<float>(contrastSpin_->value()) / 100.f));
    loading_ = false;
  }
  pushParamsFromWidgets();
}

}  // namespace tuxels
