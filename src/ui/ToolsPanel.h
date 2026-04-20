#pragma once

#include <QColor>
#include <QDockWidget>

#include "tools/ToolId.h"

class QFrame;
class QLabel;
class QSlider;
class QSpinBox;
class QToolButton;
class QWidget;

namespace tuxels {

class BrushTool;

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

  // Reflect the currently-active tool in the picker UI (without emitting
  // toolPicked). Called after MainWindow switches tools via a keyboard
  // shortcut so the button state stays in sync.
  void setActiveTool(ToolId id);

 signals:
  // Fired when the user clicks a picker button. MainWindow wires this to
  // actually swap the active tool on CanvasView.
  void toolPicked(ToolId id);

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

 private:
  void applyFgToBrush();
  void updateSwatchColors();

  BrushTool* brush_ = nullptr;
  QColor fg_ = Qt::black;
  QColor bg_ = Qt::white;
  bool suppressEdits_ = false;

  QToolButton* pickBrushBtn_ = nullptr;
  QToolButton* pickMarqueeBtn_ = nullptr;
  QWidget* brushGroup_ = nullptr;

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
