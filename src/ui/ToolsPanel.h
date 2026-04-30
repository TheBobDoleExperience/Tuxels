#pragma once

#include <QColor>
#include <QDockWidget>
#include <unordered_map>

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
class CollapsibleSection;
class LassoTool;
class MagicWandTool;
class PolyLassoTool;
class SelectByColorTool;

// Left-side dock: a vertical accordion of named tool sections, plus a fixed
// foreground/background color row pinned at the top. Each section has its own
// chevron-driven collapse state; sections do NOT auto-collapse when another
// is activated, and clicking a section's header activates the tool but never
// affects collapse state. Manual chevron clicks are the only path that flips
// expansion.
//
// Public surface preserved verbatim from the M3 letter-button picker so
// MainWindow wiring is unchanged: same tool setters, same signals, same
// `setMarqueeMode` / `setWandMode` / `setLassoMode` reflectors.
class ToolsPanel : public QDockWidget {
  Q_OBJECT

 public:
  explicit ToolsPanel(QWidget* parent = nullptr);

  void setBrushTool(BrushTool* tool);
  void refreshFromBrush();

  void setBucketTool(BucketTool* tool);
  void setMagicWandTool(MagicWandTool* tool);
  void setSelectByColorTool(SelectByColorTool* tool);
  void setLassoTools(LassoTool* lasso, PolyLassoTool* polyLasso);

  // Reflect the active tool in the section header highlighting (does not
  // affect collapse state — the user's manual choices stick).
  void setActiveTool(ToolId id);

  // Reflect persistent combine modes in their respective sections' buttons
  // (without emitting *ModeChanged). Wand and SBC share state — both
  // sections' buttons mirror it. Same for Lasso / Polygonal Lasso.
  void setMarqueeMode(SelectionMode m);
  void setWandMode(SelectionMode m);
  void setLassoMode(SelectionMode m);

  // Test-only accessor: get the section widget for a given tool, or nullptr
  // if none. Lets unit tests drive simulateHeaderClick / inspect highlight
  // state without screen scraping.
  CollapsibleSection* sectionFor(ToolId id) const;

 signals:
  void toolPicked(ToolId id);
  void marqueeModeChanged(SelectionMode m);
  void wandModeChanged(SelectionMode m);
  void lassoModeChanged(SelectionMode m);

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
  void onSizeJitterChanged(int v);
  void onOpacityJitterChanged(int v);
  void onSpacingChanged(int v);
  void onBucketToleranceChanged(int v);
  void onBucketOpacityChanged(int v);
  void onWandToleranceChanged(int v);

 private:
  void applyFgToBrush();
  void updateSwatchColors();

  // M7-S3: per-section expand/collapse persistence. Keyed by stable
  // string per `ToolId` (not the enum ordinal, which is implementation
  // detail). Loaded once in ctor; saved on every chevron toggle.
  void loadSectionStates();
  void saveSectionState(ToolId id, bool expanded);

  QWidget* buildMoveBody(QWidget* parent);
  QWidget* buildMarqueeBody(QWidget* parent);
  QWidget* buildLassoBody(QWidget* parent, bool poly);
  QWidget* buildWandBody(QWidget* parent, bool sbc);
  QWidget* buildCropBody(QWidget* parent);
  QWidget* buildBrushBody(QWidget* parent);
  QWidget* buildBucketBody(QWidget* parent);
  QWidget* buildTransformBody(QWidget* parent);

  CollapsibleSection* addSection(QWidget* root, ToolId id, const QString& name,
                                 const QString& shortcut, QWidget* body);

  BrushTool* brush_ = nullptr;
  BucketTool* bucket_ = nullptr;
  MagicWandTool* wand_ = nullptr;
  SelectByColorTool* selectByColor_ = nullptr;
  LassoTool* lasso_ = nullptr;
  PolyLassoTool* polyLasso_ = nullptr;

  QColor fg_ = Qt::black;
  QColor bg_ = Qt::white;
  bool suppressEdits_ = false;

  QFrame* fgSwatch_ = nullptr;
  QFrame* bgSwatch_ = nullptr;
  QToolButton* swapBtn_ = nullptr;
  QToolButton* resetBtn_ = nullptr;

  // Sections, indexed by ToolId. Owned by the dock's central widget.
  std::unordered_map<int, CollapsibleSection*> sections_;

  // Brush body widgets
  QSlider* sizeSlider_ = nullptr;
  QSpinBox* sizeSpin_ = nullptr;
  QSlider* hardnessSlider_ = nullptr;
  QLabel* hardnessLabel_ = nullptr;
  QSlider* opacitySlider_ = nullptr;
  QLabel* opacityLabel_ = nullptr;
  QSlider* flowSlider_ = nullptr;
  QLabel* flowLabel_ = nullptr;
  QSlider* sizeJitterSlider_ = nullptr;
  QLabel* sizeJitterLabel_ = nullptr;
  QSlider* opacityJitterSlider_ = nullptr;
  QLabel* opacityJitterLabel_ = nullptr;
  QSlider* spacingSlider_ = nullptr;
  QLabel* spacingLabel_ = nullptr;

  // Bucket body widgets
  QSlider* bucketToleranceSlider_ = nullptr;
  QLabel* bucketToleranceLabel_ = nullptr;
  QSlider* bucketOpacitySlider_ = nullptr;
  QLabel* bucketOpacityLabel_ = nullptr;

  // Wand body widgets (shared sliders between Wand and SBC sections; each
  // section gets its own QSlider/Label pair, kept in sync via setWandMode).
  QSlider* wandToleranceSlider_ = nullptr;
  QLabel* wandToleranceLabel_ = nullptr;
  QSlider* sbcToleranceSlider_ = nullptr;
  QLabel* sbcToleranceLabel_ = nullptr;

  // Combine-mode buttons. Marquee owns its own; Lasso / PolyLasso each have
  // their own row but share the lasso state; Wand / SBC each have their own
  // row but share the wand state.
  QToolButton* marqueeReplaceBtn_ = nullptr;
  QToolButton* marqueeAddBtn_ = nullptr;
  QToolButton* marqueeSubtractBtn_ = nullptr;
  QToolButton* marqueeIntersectBtn_ = nullptr;

  QToolButton* lassoReplaceBtn_ = nullptr;
  QToolButton* lassoAddBtn_ = nullptr;
  QToolButton* lassoSubtractBtn_ = nullptr;
  QToolButton* lassoIntersectBtn_ = nullptr;
  QToolButton* polyLassoReplaceBtn_ = nullptr;
  QToolButton* polyLassoAddBtn_ = nullptr;
  QToolButton* polyLassoSubtractBtn_ = nullptr;
  QToolButton* polyLassoIntersectBtn_ = nullptr;

  QToolButton* wandReplaceBtn_ = nullptr;
  QToolButton* wandAddBtn_ = nullptr;
  QToolButton* wandSubtractBtn_ = nullptr;
  QToolButton* wandIntersectBtn_ = nullptr;
  QToolButton* sbcReplaceBtn_ = nullptr;
  QToolButton* sbcAddBtn_ = nullptr;
  QToolButton* sbcSubtractBtn_ = nullptr;
  QToolButton* sbcIntersectBtn_ = nullptr;
};

}  // namespace tuxels
