#pragma once

#include <QWidget>

#include "layers/HueSaturation.h"

class QDoubleSpinBox;
class QSlider;

namespace tuxels {

// Non-modal Hue/Saturation editor for the Properties dock (M4-S4).
// Mirrors PropertiesPaneLevels' shape: three slider+spinbox pairs (hue,
// saturation, lightness) with the same snapshot-on-press, commit-on-
// release discipline. No histogram backdrop — Hue/Sat doesn't use one.
class PropertiesPaneHueSat : public QWidget {
  Q_OBJECT

 public:
  explicit PropertiesPaneHueSat(QWidget* parent = nullptr);

  void bind(HueSaturation* layer);
  void unbind();

  HueSaturation* boundLayer() const { return layer_; }
  const HueSaturationParams& paramsBefore() const { return paramsBefore_; }

  // Test hooks
  void simulateSliderPressForTest();
  void simulateSliderReleaseForTest();
  QDoubleSpinBox* hueSpinForTest() const { return hueSpin_; }
  QSlider* hueSliderForTest() const { return hueSlider_; }

 signals:
  void previewChanged();
  void commitRequested(HueSaturation* layer, HueSaturationParams before,
                       HueSaturationParams after);

 private slots:
  void onSliderPressed();
  void onSliderReleased();
  void onSpinEditingFinished();
  void onHueChanged();
  void onSaturationChanged();
  void onLightnessChanged();

 private:
  void loadFromLayer();
  void pushParamsFromWidgets();
  void snapshotBefore();
  bool paramsDifferFromBefore() const;

  HueSaturation* layer_ = nullptr;
  HueSaturationParams paramsBefore_{};
  bool loading_ = false;

  QSlider* hueSlider_ = nullptr;
  QDoubleSpinBox* hueSpin_ = nullptr;
  QSlider* satSlider_ = nullptr;
  QDoubleSpinBox* satSpin_ = nullptr;
  QSlider* lightSlider_ = nullptr;
  QDoubleSpinBox* lightSpin_ = nullptr;
};

}  // namespace tuxels
