#pragma once

#include <QMainWindow>
#include <memory>

#include "compositor/BlendMode.h"
#include "core/Document.h"
#include "tools/ToolId.h"

namespace tuxels {

class BrushTool;
class CanvasView;
class LayerBase;
class LayersPanel;
class MarqueeTool;
class ToolsPanel;
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
  void onAddLayerMask();
  void onDeleteLayerMask();
  void onLayerPaintTargetChange(LayerBase* layer, PaintTarget target);
  void onLayerMaskEnabledToggle(LayerBase* layer, bool oldVal, bool newVal);
  void onLayerDeleteMaskRequest(LayerBase* layer);
  void onSelectAll();
  void onDeselect();
  void onSelectInverse();
  void onToolPicked(ToolId id);

 private:
  void buildMenus();
  void buildDocks();
  void setDocument(std::unique_ptr<Document> doc);
  void populateSampleDocument();
  void refreshAfterUndoRedo(Rect dirtyRect = {});
  void setActiveTool(ToolId id);

  std::unique_ptr<Document> doc_;
  std::unique_ptr<BrushTool> brushTool_;
  std::unique_ptr<MarqueeTool> marqueeTool_;
  std::unique_ptr<UndoStack> undoStack_;
  CanvasView* canvas_ = nullptr;
  LayersPanel* layersPanel_ = nullptr;
  ToolsPanel* toolsPanel_ = nullptr;
  ToolId activeToolId_ = ToolId::Brush;
};

}  // namespace tuxels
