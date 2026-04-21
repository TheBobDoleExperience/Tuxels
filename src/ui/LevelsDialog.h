#pragma once

#include <QDialog>
#include <array>

#include "core/Histogram.h"
#include "layers/LevelsAdjustment.h"

class QComboBox;
class QDoubleSpinBox;
class QSlider;

namespace tuxels {

class LevelsHistogramView;

// Modal editor for a LevelsAdjustment. The dialog owns no layer — it mutates
// the caller's `layer` in place for live preview (emitting `previewChanged`
// after every slider edit) and snapshots the on-open params so Cancel can
// restore them. OK keeps whatever is currently on the layer; the caller is
// responsible for pushing a `LayerParamsCommand` after `exec() == Accepted`.
class LevelsDialog : public QDialog {
  Q_OBJECT

 public:
  LevelsDialog(LevelsAdjustment* layer, Histogram4x256 hist,
               QWidget* parent = nullptr);

  // Params captured when the dialog opened. Caller uses this as the `before`
  // half of the LayerParamsCommand.
  const std::array<LevelsParams, 4>& paramsBefore() const { return before_; }

 signals:
  void previewChanged();

 private slots:
  void onChannelChanged(int idx);
  void onInBlackChanged();
  void onInWhiteChanged();
  void onGammaChanged();
  void onOutBlackChanged();
  void onOutWhiteChanged();

 private:
  void loadChannelIntoWidgets(LevelsChannel ch);
  void pushParamsFromWidgets();
  void reject() override;

  LevelsAdjustment* layer_;
  std::array<LevelsParams, 4> before_{};
  LevelsChannel activeChannel_ = LevelsChannel::Composite;
  bool loading_ = false;

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
