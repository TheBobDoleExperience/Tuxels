#include "ui/PropertiesPaneCurves.h"

#include <QComboBox>
#include <QVBoxLayout>

#include "ui/CurveEditor.h"

namespace tuxels {

PropertiesPaneCurves::PropertiesPaneCurves(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(6);

  channelCombo_ = new QComboBox(this);
  channelCombo_->addItem(tr("Composite"));
  channelCombo_->addItem(tr("Red"));
  channelCombo_->addItem(tr("Green"));
  channelCombo_->addItem(tr("Blue"));
  layout->addWidget(channelCombo_);

  editor_ = new CurveEditor(this);
  layout->addWidget(editor_, 1);

  connect(channelCombo_,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &PropertiesPaneCurves::onChannelChanged);
  connect(editor_, &CurveEditor::previewChanged, this,
          &PropertiesPaneCurves::onEditorPreviewChanged);
  connect(editor_, &CurveEditor::interactionEnded, this,
          &PropertiesPaneCurves::onEditorInteractionEnded);
}

void PropertiesPaneCurves::bind(CurvesAdjustment* layer,
                                 Histogram4x256 hist) {
  layer_ = layer;
  editor_->setHistogram(hist);
  if (!layer_) return;
  snapshotBefore();
  loadChannel(CurvesChannel::Composite);
  // Reflect channel selection in the combo silently.
  channelCombo_->blockSignals(true);
  channelCombo_->setCurrentIndex(0);
  channelCombo_->blockSignals(false);
}

void PropertiesPaneCurves::unbind() {
  layer_ = nullptr;
  editor_->setHistogram(Histogram4x256{});
  editor_->setPoints({{0.f, 0.f}, {1.f, 1.f}});
}

void PropertiesPaneCurves::snapshotBefore() {
  if (!layer_) return;
  paramsBefore_ = layer_->allPoints();
}

void PropertiesPaneCurves::loadChannel(CurvesChannel ch) {
  if (!layer_) return;
  activeChannel_ = ch;
  editor_->setChannel(static_cast<int>(ch));
  editor_->setPoints(layer_->points(ch));
}

bool PropertiesPaneCurves::pointsDifferFromBefore() const {
  if (!layer_) return false;
  const auto cur = layer_->allPoints();
  for (std::size_t ch = 0; ch < cur.size(); ++ch) {
    if (cur[ch].size() != paramsBefore_[ch].size()) return true;
    for (std::size_t i = 0; i < cur[ch].size(); ++i) {
      if (cur[ch][i].x != paramsBefore_[ch][i].x ||
          cur[ch][i].y != paramsBefore_[ch][i].y) {
        return true;
      }
    }
  }
  return false;
}

void PropertiesPaneCurves::onChannelChanged(int idx) {
  // Channel switch is not an edit — reload editor + re-snapshot so the
  // next edit's `before` matches the on-load state.
  loadChannel(static_cast<CurvesChannel>(idx));
  snapshotBefore();
}

void PropertiesPaneCurves::onEditorPreviewChanged() {
  if (!layer_) return;
  layer_->setPoints(activeChannel_, editor_->points());
  emit previewChanged();
}

void PropertiesPaneCurves::onEditorInteractionEnded() {
  if (!layer_) return;
  if (pointsDifferFromBefore()) {
    emit commitRequested(layer_, paramsBefore_, layer_->allPoints());
    snapshotBefore();
  }
}

}  // namespace tuxels
