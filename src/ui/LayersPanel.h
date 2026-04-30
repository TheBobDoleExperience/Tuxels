#pragma once

#include <QDockWidget>
#include <cstdint>
#include <vector>

#include "compositor/BlendMode.h"
#include "core/Document.h"

class QListWidget;
class QListWidgetItem;
class QToolBar;

namespace tuxels {

class Document;
class GroupLayer;
class LayerBase;
class LayerRowWidget;

// Where a drop landed relative to the target row's vertical extent. Used by
// the panel's drop dispatcher to translate (target row, mouse y) → tree
// (parent, index). M6-S1.
enum class DropZone : std::uint8_t {
  Above,  // upper third of the row → drop just above target in panel
  On,     // middle third → drop INTO target if it's a Group, else Below
  Below,  // lower third → drop just below target in panel
};

class LayersPanel : public QDockWidget {
  Q_OBJECT

 public:
  explicit LayersPanel(QWidget* parent = nullptr);

  void setDocument(Document* doc);
  void refresh();

  // Test-only accessors. The panel's rebuild walks the tree top-down with
  // depth indices; tests inspect the resulting row widgets directly.
  int rowCountForTesting() const;
  LayerRowWidget* rowAtForTesting(int index) const;

 signals:
  void addLayerRequested();
  void deleteActiveLayerRequested();
  void moveActiveLayerUpRequested();
  void moveActiveLayerDownRequested();
  void activeLayerChanged();
  // Fired when any layer property edit requires a recomposite.
  void layerMutated();
  // Fired with old→new so MainWindow can push an undoable command.
  void visibilityChangeRequested(LayerBase* layer, bool oldVal, bool newVal);
  void blendChangeRequested(LayerBase* layer, BlendMode oldMode, BlendMode newMode);
  void opacityEditCommitted(LayerBase* layer, float oldVal, float newVal);
  void paintTargetChangeRequested(LayerBase* layer, PaintTarget target);
  void maskEnabledToggleRequested(LayerBase* layer, bool oldVal, bool newVal);
  void deleteMaskRequested(LayerBase* layer);
  void editAdjustmentRequested(LayerBase* layer);
  void toggleClipToBelowRequested(LayerBase* layer);
  // M6-S1: emitted by the embedded list widget when the user drops a row.
  // `targetParentId == 0` means root. `targetIndex` is the FINAL desired
  // tree-index of the moved layer in the destination parent (post-move
  // frame; LayerTree::move accepts this directly).
  void layerDroppedRequested(LayerId movedId, LayerId targetParentId,
                             std::size_t targetIndex);

 public:
  // Public so the embedded LayerListWidget subclass can dispatch drops.
  // Translates (movedId, target row's bound layer, drop zone) into a
  // (parent, index) pair and fires `layerDroppedRequested`. Same-parent
  // no-ops are filtered out here.
  void emitLayerDrop(LayerId movedId, LayerBase* target, DropZone zone);

 private slots:
  void onCurrentRowChanged(int row);
  void onLayerRowMutated(LayerBase* layer);
  // Chevron click on a group row — flip the group's `isExpanded` flag and
  // re-walk the tree so the panel shows / hides its children.
  void onGroupChevronToggled(GroupLayer* group);
  // M6-S2: collect selected rows' layer ids and push into the Document's
  // selection set. Fired by QListWidget on every Shift / Ctrl / single
  // click selection change.
  void onSelectionChanged();

 private:
  Document* doc_ = nullptr;
  QListWidget* list_ = nullptr;
  QToolBar* toolbar_ = nullptr;
  std::vector<LayerRowWidget*> rows_;
  bool refreshing_ = false;
};

}  // namespace tuxels
