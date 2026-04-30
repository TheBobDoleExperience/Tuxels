#include "ui/PropertiesPaneGroup.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSlider>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <cmath>

#include "layers/GroupLayer.h"

namespace tuxels {

namespace {
// 14 blend modes for groups: Pass-Through first (group default + isolation
// switch), then the standard 13. Mirrors `kGroupBlendList` in
// `LayerRowWidget.cpp`. Duplicated rather than extracted because both lists
// are tiny and tying a UI header to a constant in a Qt-test-only source
// would force a header shuffle.
constexpr std::array<BlendMode, 14> kGroupBlendList = {
    BlendMode::PassThrough, BlendMode::Normal,    BlendMode::Dissolve,
    BlendMode::Darken,      BlendMode::Multiply,  BlendMode::ColorBurn,
    BlendMode::Lighten,     BlendMode::Screen,    BlendMode::ColorDodge,
    BlendMode::Overlay,     BlendMode::SoftLight, BlendMode::HardLight,
    BlendMode::Difference,  BlendMode::Exclusion,
};
}  // namespace

PropertiesPaneGroup::PropertiesPaneGroup(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(6);

  auto* form = new QFormLayout();
  form->setContentsMargins(0, 0, 0, 0);
  form->setSpacing(4);

  nameEdit_ = new QLineEdit(this);
  form->addRow(tr("Name"), nameEdit_);

  blendCombo_ = new QComboBox(this);
  for (BlendMode m : kGroupBlendList) {
    const auto sv = blendModeName(m);
    blendCombo_->addItem(QString::fromUtf8(sv.data(), static_cast<int>(sv.size())),
                         static_cast<int>(m));
  }
  form->addRow(tr("Blend"), blendCombo_);

  opacitySlider_ = new QSlider(Qt::Horizontal, this);
  opacitySlider_->setRange(0, 100);
  opacitySpin_ = new QDoubleSpinBox(this);
  opacitySpin_->setRange(0.0, 100.0);
  opacitySpin_->setDecimals(0);
  opacitySpin_->setSingleStep(1.0);
  opacitySpin_->setSuffix(QStringLiteral("%"));
  auto* opacityRow = new QHBoxLayout();
  opacityRow->addWidget(opacitySlider_, 1);
  opacityRow->addWidget(opacitySpin_);
  form->addRow(tr("Opacity"), opacityRow);

  clipCheck_ = new QCheckBox(tr("Clip to layer below"), this);
  form->addRow(QString(), clipCheck_);

  layout->addLayout(form);
  layout->addStretch(1);

  connect(nameEdit_, &QLineEdit::editingFinished, this,
          &PropertiesPaneGroup::onNameEditingFinished);
  connect(blendCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &PropertiesPaneGroup::onBlendChanged);
  connect(opacitySlider_, &QSlider::sliderPressed, this,
          &PropertiesPaneGroup::onOpacitySliderPressed);
  connect(opacitySlider_, &QSlider::sliderReleased, this,
          &PropertiesPaneGroup::onOpacitySliderReleased);
  connect(opacitySlider_, &QSlider::valueChanged, this,
          &PropertiesPaneGroup::onOpacitySliderValueChanged);
  connect(opacitySpin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
          &PropertiesPaneGroup::onOpacitySpinValueChanged);
  connect(opacitySpin_, &QDoubleSpinBox::editingFinished, this,
          &PropertiesPaneGroup::onOpacitySpinEditingFinished);
  connect(clipCheck_, &QCheckBox::toggled, this,
          &PropertiesPaneGroup::onClipToggled);
}

void PropertiesPaneGroup::bind(GroupLayer* layer) {
  layer_ = layer;
  if (!layer_) return;
  snapshotBefore();
  loadFromLayer();
}

void PropertiesPaneGroup::unbind() { layer_ = nullptr; }

void PropertiesPaneGroup::snapshotBefore() {
  if (!layer_) return;
  paramsBefore_ = currentParams();
}

GroupProperties PropertiesPaneGroup::currentParams() const {
  GroupProperties p;
  if (!layer_) return p;
  p.name = layer_->name;
  p.blend = layer_->blend;
  p.opacity = layer_->opacity;
  p.clipToBelow = layer_->clipToBelow;
  return p;
}

bool PropertiesPaneGroup::paramsDifferFromBefore() const {
  if (!layer_) return false;
  const auto cur = currentParams();
  return cur.name != paramsBefore_.name || cur.blend != paramsBefore_.blend ||
         cur.opacity != paramsBefore_.opacity ||
         cur.clipToBelow != paramsBefore_.clipToBelow;
}

void PropertiesPaneGroup::loadFromLayer() {
  if (!layer_) return;
  loading_ = true;
  nameEdit_->setText(QString::fromStdString(layer_->name));
  int idx = 0;
  for (std::size_t i = 0; i < kGroupBlendList.size(); ++i) {
    if (kGroupBlendList[i] == layer_->blend) {
      idx = static_cast<int>(i);
      break;
    }
  }
  blendCombo_->setCurrentIndex(idx);
  const int pct = std::clamp(
      static_cast<int>(std::lround(layer_->opacity * 100.f)), 0, 100);
  opacitySlider_->setValue(pct);
  opacitySpin_->setValue(static_cast<double>(pct));
  clipCheck_->setChecked(layer_->clipToBelow);
  loading_ = false;
}

void PropertiesPaneGroup::emitCommitIfDiffers() {
  if (!layer_) return;
  if (paramsDifferFromBefore()) {
    emit commitRequested(layer_, paramsBefore_, currentParams());
    snapshotBefore();
  }
}

void PropertiesPaneGroup::simulateOpacityPressForTest() {
  onOpacitySliderPressed();
}
void PropertiesPaneGroup::simulateOpacityReleaseForTest() {
  onOpacitySliderReleased();
}
void PropertiesPaneGroup::simulateNameEditingFinishedForTest() {
  onNameEditingFinished();
}

void PropertiesPaneGroup::onNameEditingFinished() {
  if (loading_ || !layer_) return;
  layer_->name = nameEdit_->text().toStdString();
  emit previewChanged();
  emitCommitIfDiffers();
}

void PropertiesPaneGroup::onBlendChanged(int index) {
  if (loading_ || !layer_) return;
  if (index < 0 || index >= static_cast<int>(kGroupBlendList.size())) return;
  layer_->blend = kGroupBlendList[index];
  emit previewChanged();
  emitCommitIfDiffers();
}

void PropertiesPaneGroup::onOpacitySliderPressed() {
  if (!layer_) return;
  snapshotBefore();
}

void PropertiesPaneGroup::onOpacitySliderReleased() {
  emitCommitIfDiffers();
}

void PropertiesPaneGroup::onOpacitySliderValueChanged(int v) {
  if (loading_ || !layer_) return;
  loading_ = true;
  opacitySpin_->setValue(static_cast<double>(v));
  loading_ = false;
  layer_->opacity = static_cast<float>(v) / 100.f;
  emit previewChanged();
}

void PropertiesPaneGroup::onOpacitySpinValueChanged(double v) {
  if (loading_ || !layer_) return;
  loading_ = true;
  opacitySlider_->setValue(static_cast<int>(std::round(v)));
  loading_ = false;
  layer_->opacity = static_cast<float>(v) / 100.f;
  emit previewChanged();
}

void PropertiesPaneGroup::onOpacitySpinEditingFinished() {
  emitCommitIfDiffers();
}

void PropertiesPaneGroup::onClipToggled(bool checked) {
  if (loading_ || !layer_) return;
  layer_->clipToBelow = checked;
  emit previewChanged();
  emitCommitIfDiffers();
}

}  // namespace tuxels
