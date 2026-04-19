#pragma once

#include <QMainWindow>
#include <memory>

namespace tuxels {

class BrushTool;
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
  void onLayerPainted();
  void onBrushSizeIncrease();
  void onBrushSizeDecrease();

 private:
  void buildMenus();
  void buildDocks();
  void setDocument(std::unique_ptr<Document> doc);
  void populateSampleDocument();

  std::unique_ptr<Document> doc_;
  std::unique_ptr<BrushTool> brushTool_;
  CanvasView* canvas_ = nullptr;
  LayersPanel* layersPanel_ = nullptr;
};

}  // namespace tuxels
