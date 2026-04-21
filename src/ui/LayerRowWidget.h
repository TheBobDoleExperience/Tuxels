#pragma once

#include <QWidget>
#include <cstdint>

#include "compositor/BlendMode.h"
#include "core/Document.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QSlider;

namespace tuxels {

class LayerBase;

// One visual row for a layer: [vis][thumb][mask?][name][blend mode][opacity].
class LayerRowWidget : public QWidget {
  Q_OBJECT

 public:
  explicit LayerRowWidget(QWidget* parent = nullptr);

  // Populate widget state from the given layer. Caller owns the layer.
  void bindToLayer(LayerBase* layer);
  LayerBase* layer() const { return layer_; }

  // Mark the row visually as the active layer.
  void setActive(bool active);
  // Which of (layer thumb, mask thumb) should be highlighted when active.
  void setPaintTarget(PaintTarget t);

 signals:
  // Emitted after a user interaction changes the bound layer. The listener
  // should request recomposite. Undo-stack recording happens via the
  // specific old→new signals below where the previous value is known.
  void layerMutated(LayerBase* layer);
  void visibilityChangeRequested(LayerBase* layer, bool oldVal, bool newVal);
  void blendChangeRequested(LayerBase* layer, BlendMode oldMode, BlendMode newMode);
  void opacityEditCommitted(LayerBase* layer, float oldVal, float newVal);
  // Click on layer or mask thumb — select that target on this layer.
  void paintTargetChangeRequested(LayerBase* layer, PaintTarget target);
  // Shift-click on mask thumb — toggle mask->enabled.
  void maskEnabledToggleRequested(LayerBase* layer, bool oldVal, bool newVal);
  // Right-click "Delete Mask" on the mask thumb.
  void deleteMaskRequested(LayerBase* layer);
  // Click on an adjustment layer's thumb — open the edit dialog instead of
  // swapping the paint target (adjustment layers have no pixel data).
  void editAdjustmentRequested(LayerBase* layer);

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private slots:
  void onVisibilityToggled(bool checked);
  void onBlendChanged(int index);
  void onOpacitySliderMoved(int sliderValue);
  void onOpacitySliderPressed();
  void onOpacitySliderReleased();

 private:
  void rebuildThumbnail();
  void rebuildMaskThumbnail();
  void updateThumbHighlight();

  LayerBase* layer_ = nullptr;
  bool blockSignals_ = false;
  bool active_ = false;
  PaintTarget paintTarget_ = PaintTarget::Layer;
  float opacityBeforeDrag_ = 1.f;

  QCheckBox* visCheck_ = nullptr;
  QLabel* thumb_ = nullptr;
  QLabel* maskThumb_ = nullptr;
  QLabel* nameLabel_ = nullptr;
  QComboBox* blendCombo_ = nullptr;
  QSlider* opacitySlider_ = nullptr;
  QLabel* opacityValue_ = nullptr;
};

}  // namespace tuxels
