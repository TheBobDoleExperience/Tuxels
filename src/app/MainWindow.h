#pragma once

#include <QMainWindow>
#include <memory>

#include "compositor/BlendMode.h"

namespace tuxels {

class BrushTool;
class CanvasView;
class Document;
class LayerBase;
class LayersPanel;
class UndoStack;

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

 private slots:
  void onFileNew();
  void onFileOpen();
  void onFileExport();
  void onEditUndo();
  void onEditRedo();
  void onLayerAdd();
  void onLayerDelete();
  void onLayerMoveUp();
  void onLayerMoveDown();
  void onLayerPanelMutated();
  void onActiveLayerChanged();
  void onLayerPainted();
  void onBrushSizeIncrease();
  void onBrushSizeDecrease();
  void onLayerVisibilityChange(LayerBase* layer, bool oldVal, bool newVal);
  void onLayerBlendChange(LayerBase* layer, BlendMode oldMode, BlendMode newMode);
  void onLayerOpacityCommit(LayerBase* layer, float oldVal, float newVal);

 private:
  void buildMenus();
  void buildDocks();
  void setDocument(std::unique_ptr<Document> doc);
  void populateSampleDocument();
  void refreshAfterUndoRedo();

  std::unique_ptr<Document> doc_;
  std::unique_ptr<BrushTool> brushTool_;
  std::unique_ptr<UndoStack> undoStack_;
  CanvasView* canvas_ = nullptr;
  LayersPanel* layersPanel_ = nullptr;
};

}  // namespace tuxels
