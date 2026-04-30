#pragma once

#include <QWidget>
#include <string>

#include "compositor/BlendMode.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QSlider;

namespace tuxels {

class GroupLayer;

// Snapshot of the four GroupLayer fields the pane edits. Used as the
// before/after payload of `commitRequested`. Mirrors the per-pane param
// structs (LevelsParams, HueSaturationParams, etc.) but groups don't have a
// dedicated subclass-params struct so we define one here.
struct GroupProperties {
  std::string name;
  BlendMode blend = BlendMode::PassThrough;
  float opacity = 1.f;
  bool clipToBelow = false;
};

// Non-modal Group properties editor for the Properties dock (M6-S0). Mirrors
// PropertiesPaneHueSat's snapshot/commit discipline: bind() snapshots the
// current values, slider press re-snapshots, slider release / spin
// editingFinished / line-edit editingFinished / combo & check-box change
// emit `commitRequested` if the layer differs from the snapshot. Combo and
// check-box mutations commit immediately (they're atomic gestures).
//
// MainWindow wraps the commit in a `LayerParamsCommand<GroupLayer,
// GroupProperties>` whose setter applies all four fields atomically — one
// undo entry per gesture.
class PropertiesPaneGroup : public QWidget {
  Q_OBJECT

 public:
  explicit PropertiesPaneGroup(QWidget* parent = nullptr);

  void bind(GroupLayer* layer);
  void unbind();

  GroupLayer* boundLayer() const { return layer_; }
  const GroupProperties& paramsBefore() const { return paramsBefore_; }

  // Test hooks
  QLineEdit* nameEditForTest() const { return nameEdit_; }
  QComboBox* blendComboForTest() const { return blendCombo_; }
  QSlider* opacitySliderForTest() const { return opacitySlider_; }
  QDoubleSpinBox* opacitySpinForTest() const { return opacitySpin_; }
  QCheckBox* clipCheckForTest() const { return clipCheck_; }
  void simulateOpacityPressForTest();
  void simulateOpacityReleaseForTest();
  void simulateNameEditingFinishedForTest();

 signals:
  void previewChanged();
  void commitRequested(GroupLayer* layer, GroupProperties before,
                       GroupProperties after);

 private slots:
  void onNameEditingFinished();
  void onBlendChanged(int index);
  void onOpacitySliderPressed();
  void onOpacitySliderReleased();
  void onOpacitySliderValueChanged(int v);
  void onOpacitySpinValueChanged(double v);
  void onOpacitySpinEditingFinished();
  void onClipToggled(bool checked);

 private:
  void loadFromLayer();
  void snapshotBefore();
  GroupProperties currentParams() const;
  bool paramsDifferFromBefore() const;
  void emitCommitIfDiffers();

  GroupLayer* layer_ = nullptr;
  GroupProperties paramsBefore_{};
  bool loading_ = false;

  QLineEdit* nameEdit_ = nullptr;
  QComboBox* blendCombo_ = nullptr;
  QSlider* opacitySlider_ = nullptr;
  QDoubleSpinBox* opacitySpin_ = nullptr;
  QCheckBox* clipCheck_ = nullptr;
};

}  // namespace tuxels
