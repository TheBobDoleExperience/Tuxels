#pragma once

#include <QColor>
#include <QDockWidget>

class QFrame;
class QLabel;
class QSlider;
class QSpinBox;
class QToolButton;

namespace tuxels {

class BrushTool;

// Left-side dock: foreground/background color swatches (click → QColorDialog,
// X swaps, D resets to black/white) and sliders for the active brush's
// size / hardness / opacity / flow. Pushes straight into the live BrushTool
// — no intermediate model, the sliders ARE the source of truth for the
// brush params until the tool system grows a richer settings store.
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
