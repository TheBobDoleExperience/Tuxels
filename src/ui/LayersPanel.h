#pragma once

#include <QDockWidget>
#include <vector>

class QListWidget;
class QListWidgetItem;
class QToolBar;

namespace tuxels {

class Document;
class LayerBase;
class LayerRowWidget;

class LayersPanel : public QDockWidget {
  Q_OBJECT

 public:
  explicit LayersPanel(QWidget* parent = nullptr);

  void setDocument(Document* doc);
  void refresh();

 signals:
  void addLayerRequested();
  void deleteActiveLayerRequested();
  void moveActiveLayerUpRequested();
  void moveActiveLayerDownRequested();
  void activeLayerChanged();
  // Fired when any layer property edit requires a recomposite.
  void layerMutated();

 private slots:
  void onCurrentRowChanged(int row);
  void onLayerRowMutated(LayerBase* layer);

 private:
  Document* doc_ = nullptr;
  QListWidget* list_ = nullptr;
  QToolBar* toolbar_ = nullptr;
  std::vector<LayerRowWidget*> rows_;
  bool refreshing_ = false;
};

}  // namespace tuxels
