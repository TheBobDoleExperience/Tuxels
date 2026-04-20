#pragma once

#include <QMainWindow>
#include <memory>

#include "compositor/BlendMode.h"
#include "core/Document.h"
#include "tools/ToolId.h"

namespace tuxels {

class BrushTool;
class BucketTool;
class CanvasView;
class CropTool;
class LassoTool;
class LayerBase;
class LayersPanel;
class MagicWandTool;
class MarqueeTool;
class MoveTool;
class PolyLassoTool;
class ToolsPanel;
class TransformTool;
class UndoStack;

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

 private slots:
  void onFileNew();
  void onFileOpen();
  void onFilePlace();
  void onFileSaveAs();
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
  void onEditFreeTransform();
  void onTransformAccept();
  void onTransformCancel();

 private:
  void buildMenus();
  void buildDocks();
  void setDocument(std::unique_ptr<Document> doc);
  void populateSampleDocument();
  void refreshAfterUndoRedo(Rect dirtyRect = {});
  void setActiveTool(ToolId id);
  // Push the transform tool's current PendingCommit onto the undo stack and
  // deactivate the tool. No-op if the tool is idle or the transform is an
  // identity. Returns true iff a command was pushed.
  bool commitTransformIfActive();

  std::unique_ptr<Document> doc_;
  std::unique_ptr<BrushTool> brushTool_;
  std::unique_ptr<MarqueeTool> marqueeTool_;
  std::unique_ptr<BucketTool> bucketTool_;
  std::unique_ptr<MagicWandTool> wandTool_;
  std::unique_ptr<CropTool> cropTool_;
  std::unique_ptr<MoveTool> moveTool_;
  std::unique_ptr<TransformTool> transformTool_;
  std::unique_ptr<LassoTool> lassoTool_;
  std::unique_ptr<PolyLassoTool> polyLassoTool_;
  std::unique_ptr<UndoStack> undoStack_;
  CanvasView* canvas_ = nullptr;
  LayersPanel* layersPanel_ = nullptr;
  ToolsPanel* toolsPanel_ = nullptr;
  ToolId activeToolId_ = ToolId::Brush;
};

}  // namespace tuxels
