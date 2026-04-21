#include "ui/HueSatDialog.h"

#include <QDialogButtonBox>
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
constexpr int kSatMin = -100;
constexpr int kSatMax = 100;

}  // namespace

HueSatDialog::HueSatDialog(HueSaturation* layer, QWidget* parent)
    : QDialog(parent), layer_(layer), before_(layer->params()) {
  setWindowTitle(tr("Hue/Saturation"));
  setModal(true);

  auto* layout = new QVBoxLayout(this);
  auto* form = new QFormLayout();

  hueSlider_ = new QSlider(Qt::Horizontal, this);
  hueSlider_->setRange(kHueMin, kHueMax);
  hueSpin_ = new QDoubleSpinBox(this);
  hueSpin_->setRange(kHueMin, kHueMax);
  hueSpin_->setDecimals(0);
  hueSpin_->setSingleStep(1.0);
  auto* hRow = new QHBoxLayout();
  hRow->addWidget(hueSlider_, 1);
  hRow->addWidget(hueSpin_);
  form->addRow(tr("Hue"), hRow);

  satSlider_ = new QSlider(Qt::Horizontal, this);
  satSlider_->setRange(kSatMin, kSatMax);
  satSpin_ = new QDoubleSpinBox(this);
  satSpin_->setRange(kSatMin, kSatMax);
  satSpin_->setDecimals(0);
  satSpin_->setSingleStep(1.0);
  auto* sRow = new QHBoxLayout();
  sRow->addWidget(satSlider_, 1);
  sRow->addWidget(satSpin_);
  form->addRow(tr("Saturation"), sRow);

  lightSlider_ = new QSlider(Qt::Horizontal, this);
  lightSlider_->setRange(kSatMin, kSatMax);
  lightSpin_ = new QDoubleSpinBox(this);
  lightSpin_->setRange(kSatMin, kSatMax);
  lightSpin_->setDecimals(0);
  lightSpin_->setSingleStep(1.0);
  auto* lRow = new QHBoxLayout();
  lRow->addWidget(lightSlider_, 1);
  lRow->addWidget(lightSpin_);
  form->addRow(tr("Lightness"), lRow);

  layout->addLayout(form);

  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttons);

  connect(hueSlider_, &QSlider::valueChanged, this,
          &HueSatDialog::onHueChanged);
  connect(hueSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &HueSatDialog::onHueChanged);
  connect(satSlider_, &QSlider::valueChanged, this,
          &HueSatDialog::onSaturationChanged);
  connect(satSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &HueSatDialog::onSaturationChanged);
  connect(lightSlider_, &QSlider::valueChanged, this,
          &HueSatDialog::onLightnessChanged);
  connect(lightSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &HueSatDialog::onLightnessChanged);

  loading_ = true;
  hueSpin_->setValue(before_.hueShift);
  hueSlider_->setValue(static_cast<int>(std::round(before_.hueShift)));
  satSpin_->setValue(before_.saturation * 100.f);
  satSlider_->setValue(static_cast<int>(std::round(before_.saturation * 100.f)));
  lightSpin_->setValue(before_.lightness * 100.f);
  lightSlider_->setValue(
      static_cast<int>(std::round(before_.lightness * 100.f)));
  loading_ = false;
}

void HueSatDialog::pushParamsFromWidgets() {
  if (loading_) return;
  HueSaturationParams p;
  p.hueShift = static_cast<float>(hueSpin_->value());
  p.saturation = static_cast<float>(satSpin_->value()) / 100.f;
  p.lightness = static_cast<float>(lightSpin_->value()) / 100.f;
  layer_->setParams(p);
  emit previewChanged();
}

void HueSatDialog::onHueChanged() {
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

void HueSatDialog::onSaturationChanged() {
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

void HueSatDialog::onLightnessChanged() {
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

void HueSatDialog::reject() {
  layer_->setParams(before_);
  emit previewChanged();
  QDialog::reject();
}

}  // namespace tuxels
