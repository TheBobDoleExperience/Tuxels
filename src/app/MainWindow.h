#pragma once

#include <QMainWindow>
#include <memory>

#include "compositor/BlendMode.h"
#include "core/Document.h"
#include "core/Histogram.h"
#include "tools/ToolId.h"

class QAction;

namespace tuxels {

class BrushTool;
class BucketTool;
class CanvasView;
class CropTool;
class LassoTool;
class LayerBase;
class LayersPanel;
class LevelsAdjustment;
class MagicWandTool;
class MarqueeTool;
class MoveTool;
class PolyLassoTool;
class PropertiesDock;
class SelectByColorTool;
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
  void onLayerNewGroup();
  void onLayerGroupActive();
  void onLayerUngroupActive();
  void onLayerAddLevels();
  void onLayerAddCurves();
  void onLayerAddBrightnessContrast();
  void onLayerAddHueSaturation();
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
  void onEditAdjustmentRequested(LayerBase* layer);
  void onToggleClipToBelow();
  void onLayerToggleClipToBelow(LayerBase* layer);
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

  // Compose the document with `layer` and everything above it hidden, then
  // compute a histogram of the result clipped to the active selection.
  // Used by Properties-dock binds + the Levels/Curves modal "open existing"
  // path for the histogram backdrop.
  Histogram4x256 histogramBelow(LayerBase* layer);
  // If the active layer is an adjustment with a Properties pane, bind the
  // dock to it. Else show the empty state.
  void bindActiveAdjustmentToDock();
  // M5-S4: enable/disable the Group Layer + Ungroup Layer menu actions
  // based on the current active-layer state. Called when the active layer
  // changes and when the Layer menu is about to show. Keyboard shortcuts
  // (Ctrl+G / Ctrl+Shift+G) inherit the QAction's enabled state.
  void updateGroupActionStates();

  // M5-S5: where a new adjustment layer should land based on the current
  // active layer. When active is a Group, the slot is inside the group at
  // the top of its children (so the adjustment affects the group's
  // existing contents). When active is a regular layer, the slot is in
  // active's parent at active+1 (above active in the same scope). When
  // there's no active, the slot is root top.
  struct AdjustmentInsertSlot {
    LayerId parentId = 0;  // 0 = root
    std::size_t index = 0;
  };
  AdjustmentInsertSlot computeAdjustmentInsertSlot(LayerId activeId) const;

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
  std::unique_ptr<SelectByColorTool> selectByColorTool_;
  std::unique_ptr<UndoStack> undoStack_;
  CanvasView* canvas_ = nullptr;
  LayersPanel* layersPanel_ = nullptr;
  ToolsPanel* toolsPanel_ = nullptr;
  PropertiesDock* propertiesDock_ = nullptr;
  ToolId activeToolId_ = ToolId::Brush;
  // Group menu actions kept as members so `updateGroupActionStates()` can
  // toggle their enabled state in sync with the active layer (keyboard
  // shortcuts honor the QAction enable bit).
  QAction* groupLayerAction_ = nullptr;
  QAction* ungroupLayerAction_ = nullptr;
};

}  // namespace tuxels
