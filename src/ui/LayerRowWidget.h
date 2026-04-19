#pragma once

#include <QWidget>
#include <cstdint>

class QCheckBox;
class QComboBox;
class QLabel;
class QSlider;

namespace tuxels {

class LayerBase;

// One visual row for a layer: [vis][thumb][name][blend mode][opacity].
class LayerRowWidget : public QWidget {
  Q_OBJECT

 public:
  explicit LayerRowWidget(QWidget* parent = nullptr);

  // Populate widget state from the given layer. Caller owns the layer.
  void bindToLayer(LayerBase* layer);
  LayerBase* layer() const { return layer_; }

  // Mark the row visually as the active layer.
  void setActive(bool active);

 signals:
  // Emitted after a user interaction changes the bound layer. The listener
  // should request recomposite + undo-stack recording.
  void layerMutated(LayerBase* layer);

 private slots:
  void onVisibilityToggled(bool checked);
  void onBlendChanged(int index);
  void onOpacityChanged(int sliderValue);

 private:
  void rebuildThumbnail();

  LayerBase* layer_ = nullptr;
  bool blockSignals_ = false;

  QCheckBox* visCheck_ = nullptr;
  QLabel* thumb_ = nullptr;
  QLabel* nameLabel_ = nullptr;
  QComboBox* blendCombo_ = nullptr;
  QSlider* opacitySlider_ = nullptr;
  QLabel* opacityValue_ = nullptr;
};

}  // namespace tuxels
