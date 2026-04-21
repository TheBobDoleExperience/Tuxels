#pragma once

#include <QDialog>

#include "layers/HueSaturation.h"

class QDoubleSpinBox;
class QSlider;

namespace tuxels {

// Modal editor for a HueSaturation adjustment. Same shape as
// BrightnessContrastDialog: snapshot-on-open, live-preview, reject-restore.
class HueSatDialog : public QDialog {
  Q_OBJECT

 public:
  explicit HueSatDialog(HueSaturation* layer, QWidget* parent = nullptr);

  const HueSaturationParams& paramsBefore() const { return before_; }

 signals:
  void previewChanged();

 private slots:
  void onHueChanged();
  void onSaturationChanged();
  void onLightnessChanged();

 private:
  void pushParamsFromWidgets();
  void reject() override;

  HueSaturation* layer_;
  HueSaturationParams before_{};
  bool loading_ = false;

  QSlider* hueSlider_ = nullptr;
  QDoubleSpinBox* hueSpin_ = nullptr;
  QSlider* satSlider_ = nullptr;
  QDoubleSpinBox* satSpin_ = nullptr;
  QSlider* lightSlider_ = nullptr;
  QDoubleSpinBox* lightSpin_ = nullptr;
};

}  // namespace tuxels
