#pragma once

#include <QWidget>
#include <array>

#include "core/Histogram.h"
#include "layers/LevelsAdjustment.h"

class QComboBox;
class QDoubleSpinBox;
class QSlider;

namespace tuxels {

class LevelsHistogramView;

// Non-modal Levels editor for the Properties dock (M4-S2). Lifts the
// LevelsDialog's body widget tree (channel combo + 5 input/output
// slider+spinbox pairs + histogram backdrop) out of the dialog frame.
//
// Snapshot/commit discipline: snapshot on `bind()` AND on each
// QSlider::sliderPressed (or QDoubleSpinBox::editingFinished for keyboard
// edits); on QSlider::sliderReleased, if values changed, emit
// `commitRequested(layer, before, after)` so the caller can push one
// LayerParamsCommand per drag. Channel combo + spin discrete edits use
// snapshot-then-commit on every value change.
//
// The pane mutates the bound layer in place during preview — so the live
// canvas recomposite (driven by the parent dock subscribing to
// `previewChanged`) reflects the in-progress edit.
class PropertiesPaneLevels : public QWidget {
  Q_OBJECT

 public:
  explicit PropertiesPaneLevels(QWidget* parent = nullptr);

  // Bind to a Levels layer. Snapshots `paramsBefore_` for the next commit.
  // `hist` paints the histogram backdrop; pass an empty histogram (total=0)
  // to skip drawing.
  void bind(LevelsAdjustment* layer, Histogram4x256 hist);
  void unbind();

  LevelsAdjustment* boundLayer() const { return layer_; }
  const std::array<LevelsParams, 4>& paramsBefore() const {
    return paramsBefore_;
  }

  // Test hooks — Qt's QSlider only emits sliderPressed / sliderReleased on
  // real user interaction, so unit tests need a programmatic way to bracket
  // a drag. The slider/spin getters let tests drive value changes directly
  // (QSlider::setValue emits valueChanged, which fires the chain).
  void simulateSliderPressForTest();
  void simulateSliderReleaseForTest();
  QSlider* inBlackSliderForTest() const { return inBlackSlider_; }
  QDoubleSpinBox* inBlackSpinForTest() const { return inBlackSpin_; }
  QSlider* gammaSliderForTest() const { return gammaSlider_; }
  QComboBox* channelComboForTest() const { return channelCombo_; }

 signals:
  void previewChanged();
  void commitRequested(LevelsAdjustment* layer,
                       std::array<LevelsParams, 4> before,
                       std::array<LevelsParams, 4> after);

 private slots:
  void onChannelChanged(int idx);
  void onSliderPressed();
  void onSliderReleased();
  void onSpinEditingFinished();
  void onInBlackChanged();
  void onInWhiteChanged();
  void onGammaChanged();
  void onOutBlackChanged();
  void onOutWhiteChanged();

 private:
  void loadChannelIntoWidgets(LevelsChannel ch);
  void pushParamsFromWidgets();
  void snapshotBefore();
  bool paramsDifferFromBefore() const;

  LevelsAdjustment* layer_ = nullptr;
  std::array<LevelsParams, 4> paramsBefore_{};
  LevelsChannel activeChannel_ = LevelsChannel::Composite;
  bool loading_ = false;
  bool dragging_ = false;

  QComboBox* channelCombo_ = nullptr;
  LevelsHistogramView* hist_ = nullptr;

  QSlider* inBlackSlider_ = nullptr;
  QDoubleSpinBox* inBlackSpin_ = nullptr;
  QSlider* gammaSlider_ = nullptr;
  QDoubleSpinBox* gammaSpin_ = nullptr;
  QSlider* inWhiteSlider_ = nullptr;
  QDoubleSpinBox* inWhiteSpin_ = nullptr;
  QSlider* outBlackSlider_ = nullptr;
  QDoubleSpinBox* outBlackSpin_ = nullptr;
  QSlider* outWhiteSlider_ = nullptr;
  QDoubleSpinBox* outWhiteSpin_ = nullptr;
};

}  // namespace tuxels
