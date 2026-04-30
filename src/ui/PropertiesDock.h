#pragma once

#include <QDockWidget>
#include <array>

#include "core/Histogram.h"
#include "layers/BrightnessContrast.h"
#include "layers/CurvesAdjustment.h"
#include "layers/GroupLayer.h"
#include "layers/HueSaturation.h"
#include "layers/LevelsAdjustment.h"
#include "ui/PropertiesPaneGroup.h"

class QStackedWidget;
class QWidget;

namespace tuxels {

class LayerBase;
class PropertiesPaneBrightnessContrast;
class PropertiesPaneCurves;
class PropertiesPaneGroup;
class PropertiesPaneHueSat;
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
  // Bind a Curves layer + histogram backdrop (M4-S3).
  void bindCurves(CurvesAdjustment* layer, Histogram4x256 hist);
  // Bind Hue/Saturation (M4-S4). No histogram.
  void bindHueSat(HueSaturation* layer);
  // Bind Brightness/Contrast (M4-S4). No histogram.
  void bindBrightnessContrast(BrightnessContrast* layer);
  // Bind a Group layer (M6-S0). Exposes name + blend + opacity + clip-to-
  // below in a single pane. No histogram.
  void bindGroup(GroupLayer* layer);
  // Show the empty state. Called when the active layer is null or non-
  // adjustment.
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
  void curvesCommitRequested(CurvesAdjustment* layer,
                             CurvesAdjustment::PointsArray before,
                             CurvesAdjustment::PointsArray after);
  void hueSatCommitRequested(HueSaturation* layer,
                             HueSaturationParams before,
                             HueSaturationParams after);
  void brightnessContrastCommitRequested(BrightnessContrast* layer,
                                          BrightnessContrastParams before,
                                          BrightnessContrastParams after);
  // Re-emitted from PropertiesPaneGroup on commit-on-release (M6-S0).
  // MainWindow wraps in a `LayerParamsCommand<GroupLayer, GroupProperties>`.
  void groupCommitRequested(GroupLayer* layer, GroupProperties before,
                            GroupProperties after);

 private:
  QStackedWidget* stack_ = nullptr;
  QWidget* emptyPage_ = nullptr;
  PropertiesPaneLevels* levelsPane_ = nullptr;
  PropertiesPaneCurves* curvesPane_ = nullptr;
  PropertiesPaneHueSat* hueSatPane_ = nullptr;
  PropertiesPaneBrightnessContrast* brightnessContrastPane_ = nullptr;
  PropertiesPaneGroup* groupPane_ = nullptr;
};

}  // namespace tuxels
