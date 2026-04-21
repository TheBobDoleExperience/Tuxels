#include "ui/BrightnessContrastDialog.h"

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

// Sliders carry integer positions [-100, 100]; spin boxes carry the same
// integer value. Params are divided by 100 for the float model (±1 range).
constexpr int kSliderMin = -100;
constexpr int kSliderMax = 100;

int paramToSlider(float v) {
  const float s = std::clamp(v * 100.f, static_cast<float>(kSliderMin),
                             static_cast<float>(kSliderMax));
  return static_cast<int>(std::round(s));
}

}  // namespace

BrightnessContrastDialog::BrightnessContrastDialog(BrightnessContrast* layer,
                                                   QWidget* parent)
    : QDialog(parent), layer_(layer), before_(layer->params()) {
  setWindowTitle(tr("Brightness/Contrast"));
  setModal(true);

  auto* layout = new QVBoxLayout(this);
  auto* form = new QFormLayout();

  brightnessSlider_ = new QSlider(Qt::Horizontal, this);
  brightnessSlider_->setRange(kSliderMin, kSliderMax);
  brightnessSpin_ = new QDoubleSpinBox(this);
  brightnessSpin_->setRange(kSliderMin, kSliderMax);
  brightnessSpin_->setDecimals(0);
  brightnessSpin_->setSingleStep(1.0);
  auto* bRow = new QHBoxLayout();
  bRow->addWidget(brightnessSlider_, 1);
  bRow->addWidget(brightnessSpin_);
  form->addRow(tr("Brightness"), bRow);

  contrastSlider_ = new QSlider(Qt::Horizontal, this);
  contrastSlider_->setRange(kSliderMin, kSliderMax);
  contrastSpin_ = new QDoubleSpinBox(this);
  contrastSpin_->setRange(kSliderMin, kSliderMax);
  contrastSpin_->setDecimals(0);
  contrastSpin_->setSingleStep(1.0);
  auto* cRow = new QHBoxLayout();
  cRow->addWidget(contrastSlider_, 1);
  cRow->addWidget(contrastSpin_);
  form->addRow(tr("Contrast"), cRow);

  layout->addLayout(form);

  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttons);

  connect(brightnessSlider_, &QSlider::valueChanged, this,
          &BrightnessContrastDialog::onBrightnessChanged);
  connect(brightnessSpin_,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &BrightnessContrastDialog::onBrightnessChanged);
  connect(contrastSlider_, &QSlider::valueChanged, this,
          &BrightnessContrastDialog::onContrastChanged);
  connect(contrastSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &BrightnessContrastDialog::onContrastChanged);

  loading_ = true;
  brightnessSpin_->setValue(before_.brightness * 100.f);
  brightnessSlider_->setValue(paramToSlider(before_.brightness));
  contrastSpin_->setValue(before_.contrast * 100.f);
  contrastSlider_->setValue(paramToSlider(before_.contrast));
  loading_ = false;
}

void BrightnessContrastDialog::pushParamsFromWidgets() {
  if (loading_) return;
  BrightnessContrastParams p;
  p.brightness = static_cast<float>(brightnessSpin_->value()) / 100.f;
  p.contrast = static_cast<float>(contrastSpin_->value()) / 100.f;
  layer_->setParams(p);
  emit previewChanged();
}

void BrightnessContrastDialog::onBrightnessChanged() {
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

void BrightnessContrastDialog::onContrastChanged() {
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

void BrightnessContrastDialog::reject() {
  layer_->setParams(before_);
  emit previewChanged();
  QDialog::reject();
}

}  // namespace tuxels
