#include "ui/LayerRowWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPalette>
#include <QPixmap>
#include <QSlider>
#include <algorithm>
#include <cmath>

#include "compositor/BlendMode.h"
#include "core/Tile.h"
#include "core/TuxImage.h"
#include "layers/LayerBase.h"
#include "layers/PixelLayer.h"

namespace tuxels {

namespace {
constexpr int kThumbPx = 40;
constexpr std::array<BlendMode, kImplementedBlendModeCount> kBlendList = {
    BlendMode::Normal,     BlendMode::Dissolve,   BlendMode::Darken,
    BlendMode::Multiply,   BlendMode::ColorBurn,  BlendMode::Lighten,
    BlendMode::Screen,     BlendMode::ColorDodge, BlendMode::Overlay,
    BlendMode::SoftLight,  BlendMode::HardLight,  BlendMode::Difference,
    BlendMode::Exclusion,
};
}  // namespace

LayerRowWidget::LayerRowWidget(QWidget* parent) : QWidget(parent) {
  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(4, 2, 4, 2);
  layout->setSpacing(6);

  visCheck_ = new QCheckBox(this);
  visCheck_->setToolTip(tr("Toggle visibility"));
  visCheck_->setChecked(true);
  layout->addWidget(visCheck_);

  thumb_ = new QLabel(this);
  thumb_->setFixedSize(kThumbPx, kThumbPx);
  thumb_->setFrameStyle(QFrame::StyledPanel);
  layout->addWidget(thumb_);

  nameLabel_ = new QLabel(this);
  nameLabel_->setMinimumWidth(90);
  layout->addWidget(nameLabel_, /*stretch=*/1);

  blendCombo_ = new QComboBox(this);
  for (BlendMode m : kBlendList) {
    blendCombo_->addItem(QString::fromUtf8(blendModeName(m).data(),
                                           static_cast<int>(blendModeName(m).size())),
                         static_cast<int>(m));
  }
  blendCombo_->setMinimumWidth(110);
  layout->addWidget(blendCombo_);

  opacitySlider_ = new QSlider(Qt::Horizontal, this);
  opacitySlider_->setRange(0, 100);
  opacitySlider_->setValue(100);
  opacitySlider_->setFixedWidth(90);
  layout->addWidget(opacitySlider_);

  opacityValue_ = new QLabel(tr("100%"), this);
  opacityValue_->setMinimumWidth(36);
  layout->addWidget(opacityValue_);

  connect(visCheck_, &QCheckBox::toggled, this, &LayerRowWidget::onVisibilityToggled);
  connect(blendCombo_, qOverload<int>(&QComboBox::currentIndexChanged),
          this, &LayerRowWidget::onBlendChanged);
  connect(opacitySlider_, &QSlider::valueChanged,
          this, &LayerRowWidget::onOpacityChanged);
}

void LayerRowWidget::bindToLayer(LayerBase* layer) {
  layer_ = layer;
  blockSignals_ = true;
  if (layer_) {
    visCheck_->setChecked(layer_->visible);
    nameLabel_->setText(QString::fromStdString(layer_->name));
    opacitySlider_->setValue(
        std::clamp(static_cast<int>(std::lround(layer_->opacity * 100.f)), 0, 100));
    opacityValue_->setText(QStringLiteral("%1%").arg(opacitySlider_->value()));
    int idx = 0;
    for (std::size_t i = 0; i < kBlendList.size(); ++i) {
      if (kBlendList[i] == layer_->blend) { idx = static_cast<int>(i); break; }
    }
    blendCombo_->setCurrentIndex(idx);
    rebuildThumbnail();
  }
  blockSignals_ = false;
}

void LayerRowWidget::setActive(bool active) {
  QPalette pal = palette();
  if (active) {
    pal.setColor(QPalette::Window, QColor(60, 120, 200));
    setAutoFillBackground(true);
  } else {
    setAutoFillBackground(false);
  }
  setPalette(pal);
  update();
}

void LayerRowWidget::onVisibilityToggled(bool checked) {
  if (blockSignals_ || !layer_) return;
  layer_->visible = checked;
  emit layerMutated(layer_);
}

void LayerRowWidget::onBlendChanged(int index) {
  if (blockSignals_ || !layer_) return;
  if (index < 0 || static_cast<std::size_t>(index) >= kBlendList.size()) return;
  layer_->blend = kBlendList[static_cast<std::size_t>(index)];
  emit layerMutated(layer_);
}

void LayerRowWidget::onOpacityChanged(int sliderValue) {
  opacityValue_->setText(QStringLiteral("%1%").arg(sliderValue));
  if (blockSignals_ || !layer_) return;
  layer_->opacity = static_cast<float>(sliderValue) / 100.f;
  emit layerMutated(layer_);
}

void LayerRowWidget::rebuildThumbnail() {
  if (!layer_) return;
  QImage img(kThumbPx, kThumbPx, QImage::Format_RGBA8888);
  img.fill(QColor(50, 50, 50, 255));

  auto* pl = dynamic_cast<PixelLayer*>(layer_);
  if (pl && pl->image.width() > 0 && pl->image.height() > 0) {
    const int iw = pl->image.width();
    const int ih = pl->image.height();
    for (int y = 0; y < kThumbPx; ++y) {
      for (int x = 0; x < kThumbPx; ++x) {
        const int sx = std::min(iw - 1, x * iw / kThumbPx);
        const int sy = std::min(ih - 1, y * ih / kThumbPx);
        Rgba32F p = pl->image.getPixel(sx, sy);
        auto clamp255 = [](float v) {
          v = std::clamp(v, 0.f, 1.f);
          return static_cast<uchar>(std::lround(v * 255.f));
        };
        auto* row = reinterpret_cast<uchar*>(img.scanLine(y));
        row[x * 4 + 0] = clamp255(p.r);
        row[x * 4 + 1] = clamp255(p.g);
        row[x * 4 + 2] = clamp255(p.b);
        row[x * 4 + 3] = clamp255(p.a);
      }
    }
  }
  thumb_->setPixmap(QPixmap::fromImage(img));
}

}  // namespace tuxels
