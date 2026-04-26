#pragma once

#include <QWidget>

#include "layers/BrightnessContrast.h"

class QDoubleSpinBox;
class QSlider;

namespace tuxels {

// Non-modal Brightness/Contrast editor for the Properties dock (M4-S4).
// Two slider+spinbox pairs (brightness, contrast) on the same press-
// snapshot / release-commit discipline as the other Properties panes.
class PropertiesPaneBrightnessContrast : public QWidget {
  Q_OBJECT

 public:
  explicit PropertiesPaneBrightnessContrast(QWidget* parent = nullptr);

  void bind(BrightnessContrast* layer);
  void unbind();

  BrightnessContrast* boundLayer() const { return layer_; }
  const BrightnessContrastParams& paramsBefore() const {
    return paramsBefore_;
  }

  // Test hooks
  void simulateSliderPressForTest();
  void simulateSliderReleaseForTest();
  QDoubleSpinBox* brightnessSpinForTest() const { return brightnessSpin_; }
  QSlider* brightnessSliderForTest() const { return brightnessSlider_; }

 signals:
  void previewChanged();
  void commitRequested(BrightnessContrast* layer,
                       BrightnessContrastParams before,
                       BrightnessContrastParams after);

 private slots:
  void onSliderPressed();
  void onSliderReleased();
  void onSpinEditingFinished();
  void onBrightnessChanged();
  void onContrastChanged();

 private:
  void loadFromLayer();
  void pushParamsFromWidgets();
  void snapshotBefore();
  bool paramsDifferFromBefore() const;

  BrightnessContrast* layer_ = nullptr;
  BrightnessContrastParams paramsBefore_{};
  bool loading_ = false;

  QSlider* brightnessSlider_ = nullptr;
  QDoubleSpinBox* brightnessSpin_ = nullptr;
  QSlider* contrastSlider_ = nullptr;
  QDoubleSpinBox* contrastSpin_ = nullptr;
};

}  // namespace tuxels
