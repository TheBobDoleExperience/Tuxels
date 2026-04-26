#pragma once

#include <QDockWidget>
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

 private slots:
  void onCurrentRowChanged(int row);
  void onLayerRowMutated(LayerBase* layer);
  // Chevron click on a group row — flip the group's `isExpanded` flag and
  // re-walk the tree so the panel shows / hides its children.
  void onGroupChevronToggled(GroupLayer* group);

 private:
  Document* doc_ = nullptr;
  QListWidget* list_ = nullptr;
  QToolBar* toolbar_ = nullptr;
  std::vector<LayerRowWidget*> rows_;
  bool refreshing_ = false;
};

}  // namespace tuxels
