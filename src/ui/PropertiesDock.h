#pragma once

#include <QDockWidget>
#include <array>

#include "core/Histogram.h"
#include "layers/LevelsAdjustment.h"

class QStackedWidget;
class QWidget;

namespace tuxels {

class LayerBase;
class PropertiesPaneLevels;

// Right-side dock that hosts non-modal property editors for adjustment
// layers (M4-S2). Owns a `QStackedWidget` whose pages are per-adjustment-
// type panes plus an empty "no selection" page. MainWindow drives the
// type dispatch via `bindLevels(...)` / `bindNothing(...)` etc.; the dock
// re-emits each pane's `commitRequested` so MainWindow can wrap it in a
// `LayerParamsCommand`.
//
// Future steps add `bindCurves` (S3) and `bindHueSat` / `bindBrightness`
// (S4). The empty page is shown when the active layer isn't an adjustment.
class PropertiesDock : public QDockWidget {
  Q_OBJECT

 public:
  explicit PropertiesDock(QWidget* parent = nullptr);

  // Bind a Levels layer + its histogram backdrop. MainWindow computes the
  // histogram from the composite below the layer (matching the M3 dialog
  // behavior) and passes it in.
  void bindLevels(LevelsAdjustment* layer, Histogram4x256 hist);
  // Show the empty state. Called when the active layer is null or non-
  // adjustment (or an adjustment kind not yet ported to a pane).
  void bindNothing();

 signals:
  // Re-emitted from the active pane on each preview tick. MainWindow wires
  // this to `canvas_->requestRecomposite()`.
  void previewChanged();
  // Re-emitted from PropertiesPaneLevels on commit-on-release. MainWindow
  // wraps in `LayerParamsCommand<LevelsAdjustment, std::array<LevelsParams, 4>>`.
  void levelsCommitRequested(LevelsAdjustment* layer,
                             std::array<LevelsParams, 4> before,
                             std::array<LevelsParams, 4> after);

 private:
  QStackedWidget* stack_ = nullptr;
  QWidget* emptyPage_ = nullptr;
  PropertiesPaneLevels* levelsPane_ = nullptr;
};

}  // namespace tuxels
