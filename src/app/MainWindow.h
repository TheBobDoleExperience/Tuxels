#pragma once

#include <QMainWindow>
#include <memory>

namespace tuxels {

class CanvasView;
class Document;
class LayersPanel;

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

 private slots:
  void onFileNew();
  void onFileOpen();
  void onFileExport();
  void onLayerAdd();
  void onLayerDelete();
  void onLayerMoveUp();
  void onLayerMoveDown();
  void onLayerPanelMutated();
  void onActiveLayerChanged();

 private:
  void buildMenus();
  void buildDocks();
  void setDocument(std::unique_ptr<Document> doc);
  void populateSampleDocument();

  std::unique_ptr<Document> doc_;
  CanvasView* canvas_ = nullptr;
  LayersPanel* layersPanel_ = nullptr;
};

}  // namespace tuxels
