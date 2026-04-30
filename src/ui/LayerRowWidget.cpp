#include "ui/LayerRowWidget.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QSlider>
#include <QStyle>
#include <algorithm>
#include <array>
#include <cmath>

#include "compositor/BlendMode.h"
#include "core/Tile.h"
#include "core/TuxImage.h"
#include "layers/GroupLayer.h"
#include "layers/LayerBase.h"
#include "layers/LayerMask.h"
#include "layers/PixelLayer.h"

namespace tuxels {

namespace {
constexpr int kThumbPx = 40;
constexpr int kIndentPx = 12;
// Per-pixel-and-adjustment blend list — does NOT include Pass-Through (it
// only makes sense for groups). 13 entries.
constexpr std::array<BlendMode, 13> kPixelBlendList = {
    BlendMode::Normal,     BlendMode::Dissolve,   BlendMode::Darken,
    BlendMode::Multiply,   BlendMode::ColorBurn,  BlendMode::Lighten,
    BlendMode::Screen,     BlendMode::ColorDodge, BlendMode::Overlay,
    BlendMode::SoftLight,  BlendMode::HardLight,  BlendMode::Difference,
    BlendMode::Exclusion,
};
// Group blend list — Pass-Through first (the PS default), followed by the
// regular modes. Switching to a non-Pass-Through entry isolates the group.
constexpr std::array<BlendMode, 14> kGroupBlendList = {
    BlendMode::PassThrough,
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

  // Group expand/collapse chevron (M5-S3). Visible only when bound to a
  // GroupLayer. Click toggles `isExpanded` via a signal the panel handles
  // (so the panel can re-walk the tree and decide which rows to render).
  chevron_ = new QLabel(this);
  chevron_->setFixedWidth(14);
  chevron_->setAlignment(Qt::AlignCenter);
  chevron_->setToolTip(tr("Expand / collapse group"));
  chevron_->setStyleSheet("color: rgba(220, 220, 220, 240);");
  chevron_->setCursor(Qt::PointingHandCursor);
  chevron_->installEventFilter(this);
  chevron_->setVisible(false);
  layout->addWidget(chevron_);

  // Clip-to-below indicator (M4-S1). Only visible when the bound layer has
  // `clipToBelow == true`. Acts as a visual indent affordance — the row
  // appears nested under the layer below it (matches PS).
  clipGlyph_ = new QLabel(QStringLiteral("↳"), this);
  clipGlyph_->setFixedWidth(14);
  clipGlyph_->setAlignment(Qt::AlignCenter);
  clipGlyph_->setToolTip(
      tr("Clipped — affects only the layer immediately below"));
  clipGlyph_->setStyleSheet("color: rgba(180, 180, 180, 220);");
  clipGlyph_->setVisible(false);
  layout->addWidget(clipGlyph_);

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
  // M7-S5: double-click the label to begin in-place rename. The label
  // installs an eventFilter on `this` (LayerRowWidget) so the existing
  // eventFilter dispatch can route the gesture.
  nameLabel_->installEventFilter(this);
  layout->addWidget(nameLabel_, /*stretch=*/1);

  nameEdit_ = new QLineEdit(this);
  nameEdit_->setMinimumWidth(90);
  nameEdit_->hide();
  // editingFinished fires on Enter and on focus-loss. We also intercept
  // Escape via the eventFilter to revert without committing.
  nameEdit_->installEventFilter(this);
  layout->addWidget(nameEdit_, /*stretch=*/1);
  connect(nameEdit_, &QLineEdit::editingFinished, this,
          &LayerRowWidget::commitNameEdit);

  blendCombo_ = new QComboBox(this);
  blendCombo_->setMinimumWidth(110);
  layout->addWidget(blendCombo_);
  // Default: pixel/adjustment blend list (no Pass-Through). Re-built per
  // bind when the layer kind changes.
  rebuildBlendCombo(/*includePassThrough=*/false);

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
  // Drop any in-progress rename when binding to a different layer (the row
  // gets reused across panel rebuilds, so a stale edit could carry over).
  if (nameEdit_ && nameEdit_->isVisible()) {
    nameEdit_->hide();
    if (nameLabel_) nameLabel_->show();
  }
  if (layer_) {
    const bool isGroup = (layer_->kind() == LayerKind::Group);
    // Repopulate the blend combo if the kind transitioned (e.g. row reused
    // for a different layer). Keeps Pass-Through hidden for non-groups.
    if (isGroup != comboHasPassThrough_) {
      rebuildBlendCombo(isGroup);
    }
    visCheck_->setChecked(layer_->visible);
    nameLabel_->setText(QString::fromStdString(layer_->name));
    opacitySlider_->setValue(
        std::clamp(static_cast<int>(std::lround(layer_->opacity * 100.f)), 0, 100));
    opacityValue_->setText(QStringLiteral("%1%").arg(opacitySlider_->value()));
    int idx = 0;
    if (isGroup) {
      for (std::size_t i = 0; i < kGroupBlendList.size(); ++i) {
        if (kGroupBlendList[i] == layer_->blend) { idx = static_cast<int>(i); break; }
      }
    } else {
      for (std::size_t i = 0; i < kPixelBlendList.size(); ++i) {
        if (kPixelBlendList[i] == layer_->blend) { idx = static_cast<int>(i); break; }
      }
    }
    blendCombo_->setCurrentIndex(idx);
    if (clipGlyph_) clipGlyph_->setVisible(layer_->clipToBelow);
    if (chevron_) {
      chevron_->setVisible(isGroup);
      updateChevronGlyph();
    }
    rebuildThumbnail();
    rebuildMaskThumbnail();
    updateThumbHighlight();
  }
  blockSignals_ = false;
}

void LayerRowWidget::setIndentDepth(int depth) {
  if (depth < 0) depth = 0;
  indentDepth_ = depth;
  if (auto* lay = layout()) {
    int l, t, r, b;
    lay->getContentsMargins(&l, &t, &r, &b);
    lay->setContentsMargins(4 + depth * kIndentPx, t, r, b);
  }
}

void LayerRowWidget::rebuildBlendCombo(bool includePassThrough) {
  if (!blendCombo_) return;
  const bool wasBlocked = blendCombo_->blockSignals(true);
  blendCombo_->clear();
  if (includePassThrough) {
    for (BlendMode m : kGroupBlendList) {
      blendCombo_->addItem(
          QString::fromUtf8(blendModeName(m).data(),
                            static_cast<int>(blendModeName(m).size())),
          static_cast<int>(m));
    }
  } else {
    for (BlendMode m : kPixelBlendList) {
      blendCombo_->addItem(
          QString::fromUtf8(blendModeName(m).data(),
                            static_cast<int>(blendModeName(m).size())),
          static_cast<int>(m));
    }
  }
  blendCombo_->blockSignals(wasBlocked);
  comboHasPassThrough_ = includePassThrough;
}

void LayerRowWidget::updateChevronGlyph() {
  if (!chevron_ || !layer_ || layer_->kind() != LayerKind::Group) return;
  const auto* g = static_cast<const GroupLayer*>(layer_);
  // ▾ = expanded (children visible below); ▸ = collapsed (children hidden).
  chevron_->setText(g->isExpanded ? QStringLiteral("▾")
                                   : QStringLiteral("▸"));
}

int LayerRowWidget::blendItemCountForTesting() const {
  return blendCombo_ ? blendCombo_->count() : 0;
}

bool LayerRowWidget::isChevronVisibleForTesting() const {
  return chevron_ && chevron_->isVisibleTo(const_cast<LayerRowWidget*>(this));
}

void LayerRowWidget::simulateChevronClickForTesting() {
  if (!layer_ || layer_->kind() != LayerKind::Group) return;
  emit chevronToggled(static_cast<GroupLayer*>(layer_));
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
  const std::size_t i = static_cast<std::size_t>(index);
  BlendMode newMode;
  if (comboHasPassThrough_) {
    if (index < 0 || i >= kGroupBlendList.size()) return;
    newMode = kGroupBlendList[i];
  } else {
    if (index < 0 || i >= kPixelBlendList.size()) return;
    newMode = kPixelBlendList[i];
  }
  const BlendMode oldMode = layer_->blend;
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

void LayerRowWidget::commitNameEdit() {
  if (!layer_ || !nameEdit_ || !nameEdit_->isVisible()) return;
  const std::string newName = nameEdit_->text().toStdString();
  const std::string oldName = layer_->name;
  // Always restore the label/visible state regardless of diff, so the row
  // looks settled after Enter / focus-loss.
  nameEdit_->hide();
  nameLabel_->show();
  if (newName.empty() || newName == oldName) return;
  emit nameChangeRequested(layer_, oldName, newName);
}

bool LayerRowWidget::eventFilter(QObject* watched, QEvent* event) {
  if (!layer_) return QWidget::eventFilter(watched, event);
  // M7-S5: double-click on the name label → swap label for in-place
  // QLineEdit, focus + select-all so the user can immediately type.
  if (watched == nameLabel_ &&
      event->type() == QEvent::MouseButtonDblClick) {
    nameEdit_->setText(QString::fromStdString(layer_->name));
    nameLabel_->hide();
    nameEdit_->show();
    nameEdit_->setFocus();
    nameEdit_->selectAll();
    return true;
  }
  if (watched == nameEdit_ && event->type() == QEvent::KeyPress) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke->key() == Qt::Key_Escape) {
      // Revert without committing: hide the edit, clear focus, restore
      // the label. editingFinished may still fire on focus-loss after
      // this — commitNameEdit checks that the edit is visible, so the
      // post-revert focus-loss is a no-op.
      nameEdit_->hide();
      nameLabel_->show();
      setFocus();
      return true;
    }
  }
  if (watched == chevron_ && event->type() == QEvent::MouseButtonPress) {
    auto* me = static_cast<QMouseEvent*>(event);
    if (me->button() == Qt::LeftButton &&
        layer_->kind() == LayerKind::Group) {
      emit chevronToggled(static_cast<GroupLayer*>(layer_));
      return true;  // swallow — don't activate the row
    }
  } else if (watched == thumb_ && event->type() == QEvent::MouseButtonPress) {
    auto* me = static_cast<QMouseEvent*>(event);
    if (me->button() == Qt::LeftButton) {
      // Adjustment layers have no pixel data to paint; the thumb doubles
      // as the edit affordance. Pixel layers keep the paint-target swap.
      // Group thumb is purely decorative — no paint-target swap.
      if (layer_->kind() == LayerKind::Adjustment) {
        emit editAdjustmentRequested(layer_);
      } else if (layer_->kind() == LayerKind::Pixel) {
        emit paintTargetChangeRequested(layer_, PaintTarget::Layer);
      }
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

  // Adjustment layers have no pixel data — render a fixed-label glyph so
  // the row is visually distinct from pixel layers. S0 uses a generic
  // "fx" stub; later steps can swap in "Lv"/"Cv"/etc via dynamic_cast.
  if (layer_->kind() == LayerKind::Adjustment) {
    QPixmap pm(kThumbPx, kThumbPx);
    pm.fill(QColor(40, 40, 55, 255));
    QPainter p(&pm);
    p.setPen(QColor(220, 220, 220));
    QFont f = p.font();
    f.setBold(true);
    f.setPointSize(12);
    p.setFont(f);
    p.drawText(pm.rect(), Qt::AlignCenter, QStringLiteral("fx"));
    p.end();
    thumb_->setPixmap(pm);
    return;
  }

  // Groups (M5) — Qt standard folder icon over a tinted background. Tint
  // distinguishes groups from pixel + adjustment thumbs at a glance.
  if (layer_->kind() == LayerKind::Group) {
    QPixmap pm(kThumbPx, kThumbPx);
    pm.fill(QColor(55, 50, 40, 255));
    QIcon folderIcon = style()->standardIcon(QStyle::SP_DirIcon);
    QPixmap iconPm = folderIcon.pixmap(kThumbPx - 8, kThumbPx - 8);
    QPainter p(&pm);
    p.drawPixmap((kThumbPx - iconPm.width()) / 2,
                 (kThumbPx - iconPm.height()) / 2, iconPm);
    p.end();
    thumb_->setPixmap(pm);
    return;
  }

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

void LayerRowWidget::contextMenuEvent(QContextMenuEvent* event) {
  if (!layer_) {
    QWidget::contextMenuEvent(event);
    return;
  }
  QMenu menu(this);
  const bool clipped = layer_->clipToBelow;
  auto* clipAct = menu.addAction(clipped ? tr("Release Clipping Mask")
                                          : tr("Create Clipping Mask"));
  clipAct->setShortcut(QKeySequence("Ctrl+Alt+G"));
  QAction* chosen = menu.exec(event->globalPos());
  if (chosen == clipAct) emit toggleClipToBelowRequested(layer_);
  event->accept();
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
