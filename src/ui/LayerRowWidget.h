#pragma once

#include <QWidget>
#include <cstdint>

#include "compositor/BlendMode.h"
#include "core/Document.h"
#include "layers/LayerColorLabel.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSlider;

namespace tuxels {

class LayerBase;
class GroupLayer;

// One visual row for a layer: [vis][chevron?][clip?][thumb][mask?][name]
// [blend mode][opacity]. Groups (M5) get a chevron + folder-glyph thumb +
// "Pass Through" entry in the blend combo; non-groups omit those.
// `setIndentDepth(int)` controls left padding so nested children indent
// under their parent group (caller passes depth; widget computes margin).
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
  // Indent in tree levels (0 = root). Updates the layout's left margin so
  // children of expanded groups visually nest under their parent.
  void setIndentDepth(int depth);
  int indentDepth() const { return indentDepth_; }

  // M7-S6: external entry point for triggering the in-place rename from
  // the context menu (or other paths). Same effect as a double-click on
  // the name label.
  void beginRename();

  // Test-only accessors.
  int blendItemCountForTesting() const;
  bool blendComboHasPassThroughForTesting() const {
    return comboHasPassThrough_;
  }
  bool isChevronVisibleForTesting() const;
  void simulateChevronClickForTesting();

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
  // Right-click → "Create Clipping Mask" / "Release Clipping Mask" on the
  // row body. Toggles `clipToBelow` on the bound layer (M4-S1 PS-style
  // clip-to-layer). MainWindow wraps in a LayerOpCommand for undo.
  void toggleClipToBelowRequested(LayerBase* layer);
  // Chevron click on a group row — toggle the group's `isExpanded` flag.
  // Panel handles the side-effect (mutate flag + refresh).
  void chevronToggled(GroupLayer* group);
  // M7-S5: double-click on the name label opens an in-place QLineEdit;
  // commit (Enter / focus-loss with a non-empty diff) emits this so
  // MainWindow can wrap the rename in a LayerOpCommand for undo.
  void nameChangeRequested(LayerBase* layer, std::string oldName,
                            std::string newName);
  // M7-S6: right-click context-menu actions. The panel relays each to
  // MainWindow which sets the row's layer active + reuses the existing
  // global-action slot. `addLayerMaskRequested` is pixel-only at the
  // emit site (the menu entry is only shown for PixelLayer rows).
  void duplicateLayerRequested(LayerBase* layer);
  void deleteLayerRequested(LayerBase* layer);
  void groupLayerRequested(LayerBase* layer);
  void addLayerMaskRequested(LayerBase* layer);
  void renameLayerRequested(LayerBase* layer);
  // M8-S0: color label change from the row's context menu.
  void colorLabelChangeRequested(LayerBase* layer, LayerColorLabel oldLabel,
                                  LayerColorLabel newLabel);

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private slots:
  void onVisibilityToggled(bool checked);
  void onBlendChanged(int index);
  void onOpacitySliderMoved(int sliderValue);
  void onOpacitySliderPressed();
  void onOpacitySliderReleased();
  void commitNameEdit();

 private:
  void rebuildThumbnail();
  void rebuildMaskThumbnail();
  void rebuildBlendCombo(bool includePassThrough);
  void updateThumbHighlight();
  void updateChevronGlyph();

  LayerBase* layer_ = nullptr;
  bool blockSignals_ = false;
  bool active_ = false;
  PaintTarget paintTarget_ = PaintTarget::Layer;
  float opacityBeforeDrag_ = 1.f;
  int indentDepth_ = 0;
  bool comboHasPassThrough_ = false;

  // M8-S0: 4-px-wide colored stripe at the row's left edge. Hidden when
  // colorLabel == None.
  QWidget* colorStripe_ = nullptr;

  QCheckBox* visCheck_ = nullptr;
  QLabel* chevron_ = nullptr;
  QLabel* clipGlyph_ = nullptr;
  QLabel* thumb_ = nullptr;
  QLabel* maskThumb_ = nullptr;
  QLabel* nameLabel_ = nullptr;
  // M7-S5: in-place rename. Hidden by default; double-click on the
  // nameLabel_ shows the edit, focuses + selects all. Enter / focus-loss
  // commits via `commitNameEdit`; Escape reverts.
  QLineEdit* nameEdit_ = nullptr;
  QComboBox* blendCombo_ = nullptr;
  QSlider* opacitySlider_ = nullptr;
  QLabel* opacityValue_ = nullptr;

 protected:
  void contextMenuEvent(QContextMenuEvent* event) override;
};

}  // namespace tuxels
