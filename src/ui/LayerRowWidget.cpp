#include "ui/LayerRowWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPalette>
#include <QPixmap>
#include <QSlider>
#include <algorithm>
#include <array>
#include <cmath>

#include "compositor/BlendMode.h"
#include "core/Tile.h"
#include "core/TuxImage.h"
#include "layers/LayerBase.h"
#include "layers/LayerMask.h"
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
  thumb_->setToolTip(tr("Layer thumbnail — click to paint on layer"));
  thumb_->installEventFilter(this);
  layout->addWidget(thumb_);

  maskThumb_ = new QLabel(this);
  maskThumb_->setFixedSize(kThumbPx, kThumbPx);
  maskThumb_->setFrameStyle(QFrame::StyledPanel);
  maskThumb_->setToolTip(
      tr("Mask thumbnail — click to paint on mask, shift-click to "
         "enable/disable, right-click for options"));
  maskThumb_->installEventFilter(this);
  maskThumb_->hide();
  layout->addWidget(maskThumb_);

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
          this, &LayerRowWidget::onOpacitySliderMoved);
  connect(opacitySlider_, &QSlider::sliderPressed,
          this, &LayerRowWidget::onOpacitySliderPressed);
  connect(opacitySlider_, &QSlider::sliderReleased,
          this, &LayerRowWidget::onOpacitySliderReleased);
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
    rebuildMaskThumbnail();
    updateThumbHighlight();
  }
  blockSignals_ = false;
}

void LayerRowWidget::setActive(bool active) {
  active_ = active;
  QPalette pal = palette();
  if (active) {
    pal.setColor(QPalette::Window, QColor(60, 120, 200));
    setAutoFillBackground(true);
  } else {
    setAutoFillBackground(false);
  }
  setPalette(pal);
  updateThumbHighlight();
  update();
}

void LayerRowWidget::setPaintTarget(PaintTarget t) {
  paintTarget_ = t;
  updateThumbHighlight();
}

void LayerRowWidget::onVisibilityToggled(bool checked) {
  if (blockSignals_ || !layer_) return;
  const bool oldVal = layer_->visible;
  if (oldVal == checked) return;
  emit visibilityChangeRequested(layer_, oldVal, checked);
}

void LayerRowWidget::onBlendChanged(int index) {
  if (blockSignals_ || !layer_) return;
  if (index < 0 || static_cast<std::size_t>(index) >= kBlendList.size()) return;
  const BlendMode oldMode = layer_->blend;
  const BlendMode newMode = kBlendList[static_cast<std::size_t>(index)];
  if (oldMode == newMode) return;
  emit blendChangeRequested(layer_, oldMode, newMode);
}

void LayerRowWidget::onOpacitySliderMoved(int sliderValue) {
  opacityValue_->setText(QStringLiteral("%1%").arg(sliderValue));
  if (blockSignals_ || !layer_) return;
  layer_->opacity = static_cast<float>(sliderValue) / 100.f;
  emit layerMutated(layer_);
}

void LayerRowWidget::onOpacitySliderPressed() {
  if (!layer_) return;
  opacityBeforeDrag_ = layer_->opacity;
}

void LayerRowWidget::onOpacitySliderReleased() {
  if (!layer_) return;
  const float newVal = layer_->opacity;
  if (std::fabs(newVal - opacityBeforeDrag_) < 1e-4f) return;
  emit opacityEditCommitted(layer_, opacityBeforeDrag_, newVal);
}

bool LayerRowWidget::eventFilter(QObject* watched, QEvent* event) {
  if (!layer_) return QWidget::eventFilter(watched, event);
  if (watched == thumb_ && event->type() == QEvent::MouseButtonPress) {
    auto* me = static_cast<QMouseEvent*>(event);
    if (me->button() == Qt::LeftButton) {
      emit paintTargetChangeRequested(layer_, PaintTarget::Layer);
      return true;
    }
  } else if (watched == maskThumb_ && event->type() == QEvent::MouseButtonPress) {
    auto* me = static_cast<QMouseEvent*>(event);
    if (me->button() == Qt::LeftButton) {
      if (me->modifiers() & Qt::ShiftModifier) {
        const bool oldVal = layer_->mask && layer_->mask->enabled;
        emit maskEnabledToggleRequested(layer_, oldVal, !oldVal);
      } else {
        emit paintTargetChangeRequested(layer_, PaintTarget::Mask);
      }
      return true;
    }
    if (me->button() == Qt::RightButton) {
      QMenu menu(this);
      auto* deleteAct = menu.addAction(tr("Delete Mask"));
      QAction* chosen = menu.exec(me->globalPosition().toPoint());
      if (chosen == deleteAct) emit deleteMaskRequested(layer_);
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
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

void LayerRowWidget::rebuildMaskThumbnail() {
  if (!layer_ || !maskThumb_) return;
  if (!layer_->mask) {
    maskThumb_->hide();
    maskThumb_->clear();
    return;
  }
  const TuxImage& mi = layer_->mask->image;
  const bool disabled = !layer_->mask->enabled;
  QImage img(kThumbPx, kThumbPx, QImage::Format_RGBA8888);
  const int iw = mi.width();
  const int ih = mi.height();
  for (int y = 0; y < kThumbPx; ++y) {
    for (int x = 0; x < kThumbPx; ++x) {
      float v = 1.f;
      if (iw > 0 && ih > 0) {
        const int sx = std::min(iw - 1, x * iw / kThumbPx);
        const int sy = std::min(ih - 1, y * ih / kThumbPx);
        v = std::clamp(mi.getPixel(sx, sy).r, 0.f, 1.f);
      }
      auto* row = reinterpret_cast<uchar*>(img.scanLine(y));
      uchar r = static_cast<uchar>(std::lround(v * 255.f));
      uchar g = r;
      uchar b = r;
      if (disabled) {
        // Bake a red tint straight onto the pixmap (stylesheet background
        // wouldn't show through the opaque label pixmap).
        constexpr float kTintA = 0.45f;
        r = static_cast<uchar>(std::lround(
            (1.f - kTintA) * r + kTintA * 220.f));
        g = static_cast<uchar>(std::lround((1.f - kTintA) * g));
        b = static_cast<uchar>(std::lround((1.f - kTintA) * b));
      }
      row[x * 4 + 0] = r;
      row[x * 4 + 1] = g;
      row[x * 4 + 2] = b;
      row[x * 4 + 3] = 255;
    }
  }
  maskThumb_->setPixmap(QPixmap::fromImage(img));
  maskThumb_->show();
}

void LayerRowWidget::updateThumbHighlight() {
  const bool layerSel = active_ && paintTarget_ == PaintTarget::Layer;
  const bool maskSel = active_ && paintTarget_ == PaintTarget::Mask;
  thumb_->setStyleSheet(layerSel ? "border: 2px solid #3cf;" : "");
  if (!maskThumb_) return;
  // Disabled-mask tint is baked into the pixmap by rebuildMaskThumbnail;
  // the stylesheet only carries the active-thumb border.
  maskThumb_->setStyleSheet(maskSel ? "border: 2px solid #3cf;" : "");
}

}  // namespace tuxels
