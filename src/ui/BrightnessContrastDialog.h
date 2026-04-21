#pragma once

#include <QDialog>

#include "layers/BrightnessContrast.h"

class QDoubleSpinBox;
class QSlider;

namespace tuxels {

// Modal editor for a BrightnessContrast adjustment. Mirrors the
// LevelsDialog pattern: live-preview via `previewChanged`, snapshot params
// on open, restore via `reject()` on Cancel. Caller is responsible for
// pushing a `LayerParamsCommand<BrightnessContrast, BrightnessContrastParams>`
// after `exec() == Accepted`.
class BrightnessContrastDialog : public QDialog {
  Q_OBJECT

 public:
  explicit BrightnessContrastDialog(BrightnessContrast* layer,
                                    QWidget* parent = nullptr);

  const BrightnessContrastParams& paramsBefore() const { return before_; }

 signals:
  void previewChanged();

 private slots:
  void onBrightnessChanged();
  void onContrastChanged();

 private:
  void pushParamsFromWidgets();
  void reject() override;

  BrightnessContrast* layer_;
  BrightnessContrastParams before_{};
  bool loading_ = false;

  QSlider* brightnessSlider_ = nullptr;
  QDoubleSpinBox* brightnessSpin_ = nullptr;
  QSlider* contrastSlider_ = nullptr;
  QDoubleSpinBox* contrastSpin_ = nullptr;
};

}  // namespace tuxels
