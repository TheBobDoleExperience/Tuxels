#pragma once

#include <QColor>
#include <QDockWidget>

#include "core/SelectionMask.h"
#include "tools/ToolId.h"

class QFrame;
class QLabel;
class QSlider;
class QSpinBox;
class QToolButton;
class QWidget;

namespace tuxels {

class BrushTool;
class BucketTool;
class MagicWandTool;

// Left-side dock: tool picker (Brush / Marquee / …) plus the active tool's
// parameter controls. Brush: fg/bg swatches + size/hardness/opacity/flow.
// Non-paint tools hide the brush panel. Pushes straight into the live
// BrushTool — no intermediate model, the sliders ARE the source of truth
// for brush params until the tool system grows a richer settings store.
class ToolsPanel : public QDockWidget {
  Q_OBJECT

 public:
  explicit ToolsPanel(QWidget* parent = nullptr);

  // Attach the live BrushTool. The panel pushes changes into it and
  // reflects its current state.
  void setBrushTool(BrushTool* tool);
  // Pull fresh values from the tool (after, e.g., `[`/`]` changed the size
  // behind the panel's back) and update all widgets without triggering
  // edit signals.
  void refreshFromBrush();

  // Attach the live BucketTool. The panel pushes tolerance / opacity /
  // contiguous changes straight into it; the foreground swatch already
  // drives the fill color (same source of truth as the brush color).
  void setBucketTool(BucketTool* tool);

  // Attach the live MagicWandTool. The panel pushes tolerance and combine
  // mode into it; the tool's own pending commit is consumed by MainWindow
  // on press.
  void setMagicWandTool(MagicWandTool* tool);

  // Reflect the currently-active tool in the picker UI (without emitting
  // toolPicked). Called after MainWindow switches tools via a keyboard
  // shortcut so the button state stays in sync.
  void setActiveTool(ToolId id);

  // Reflect the marquee's persistent combine mode in the options row
  // (without emitting marqueeModeChanged). Used when the mode is driven
  // from outside the panel (e.g., future keyboard shortcuts).
  void setMarqueeMode(SelectionMode m);

  // Reflect the magic wand's persistent combine mode in its options row.
  void setWandMode(SelectionMode m);

 signals:
  // Fired when the user clicks a picker button. MainWindow wires this to
  // actually swap the active tool on CanvasView.
  void toolPicked(ToolId id);

  // Fired when the user picks a marquee combine mode (New/Add/Subtract/
  // Intersect). MainWindow wires this to MarqueeTool::setMode. Exists so
  // Subtract/Intersect stay reachable on WMs that grab Alt-drag for
  // window-move.
  void marqueeModeChanged(SelectionMode m);

  // Same idea for the magic wand — Subtract/Intersect stay reachable
  // without Alt.
  void wandModeChanged(SelectionMode m);

 public slots:
  void swapColors();
  void resetColors();

 private slots:
  void onFgSwatchClicked();
  void onBgSwatchClicked();
  void onSizeChanged(int v);
  void onHardnessChanged(int v);
  void onOpacityChanged(int v);
  void onFlowChanged(int v);
  void onBucketToleranceChanged(int v);
  void onBucketOpacityChanged(int v);
  void onWandToleranceChanged(int v);

 private:
  void applyFgToBrush();
  void updateSwatchColors();

  BrushTool* brush_ = nullptr;
  QColor fg_ = Qt::black;
  QColor bg_ = Qt::white;
  bool suppressEdits_ = false;

  QToolButton* pickBrushBtn_ = nullptr;
  QToolButton* pickMarqueeBtn_ = nullptr;
  QToolButton* pickBucketBtn_ = nullptr;
  QToolButton* pickWandBtn_ = nullptr;
  QToolButton* pickCropBtn_ = nullptr;
  QWidget* brushGroup_ = nullptr;
  QWidget* marqueeGroup_ = nullptr;
  QWidget* bucketGroup_ = nullptr;
  QWidget* wandGroup_ = nullptr;
  QToolButton* marqueeReplaceBtn_ = nullptr;
  QToolButton* marqueeAddBtn_ = nullptr;
  QToolButton* marqueeSubtractBtn_ = nullptr;
  QToolButton* marqueeIntersectBtn_ = nullptr;
  QToolButton* wandReplaceBtn_ = nullptr;
  QToolButton* wandAddBtn_ = nullptr;
  QToolButton* wandSubtractBtn_ = nullptr;
  QToolButton* wandIntersectBtn_ = nullptr;

  BucketTool* bucket_ = nullptr;
  QSlider* bucketToleranceSlider_ = nullptr;
  QLabel* bucketToleranceLabel_ = nullptr;
  QSlider* bucketOpacitySlider_ = nullptr;
  QLabel* bucketOpacityLabel_ = nullptr;

  MagicWandTool* wand_ = nullptr;
  QSlider* wandToleranceSlider_ = nullptr;
  QLabel* wandToleranceLabel_ = nullptr;

  QFrame* fgSwatch_ = nullptr;
  QFrame* bgSwatch_ = nullptr;
  QToolButton* swapBtn_ = nullptr;
  QToolButton* resetBtn_ = nullptr;

  QSlider* sizeSlider_ = nullptr;
  QSpinBox* sizeSpin_ = nullptr;
  QSlider* hardnessSlider_ = nullptr;
  QLabel* hardnessLabel_ = nullptr;
  QSlider* opacitySlider_ = nullptr;
  QLabel* opacityLabel_ = nullptr;
  QSlider* flowSlider_ = nullptr;
  QLabel* flowLabel_ = nullptr;
};

}  // namespace tuxels
