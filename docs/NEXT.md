# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M4 active.** S0 + S1 shipped 2026-04-26. 285 tests across 30
executables. Next is **M4-S2: Properties dock + Levels port.**

S2 work (per `/home/james/.claude/plans/quirky-napping-koala.md`):

1. **`src/ui/PropertiesDock.{h,cpp}`** (new) — `QDockWidget` parked on
   `Qt::RightDockWidgetArea`, default tab-stacked under `LayersPanel`
   via `tabifyDockWidget(layersPanel_, propertiesDock_)`. Owns a
   `QStackedWidget` whose pages are per-adjustment-type panes (Levels
   for S2; Curves/H-S/B&C in S3/S4) plus an "empty" page that shows
   "Select an adjustment layer to edit its properties." API:
   `bind(LayerBase* layer)` — `dynamic_cast` ladder picks the matching
   pane and switches the stack; nullptr = empty page.
2. **`src/ui/PropertiesPaneLevels.{h,cpp}`** (new) — bare `QWidget`
   (Q_OBJECT). Channel combo + 5 input/output sliders + spin boxes +
   histogram backdrop, lifted from `LevelsDialog`. No `QDialog`, no
   OK/Cancel, no `reject()`.
3. **Snapshot/commit discipline** — pane carries `paramsBefore_`
   snapshot taken on `bind(LevelsAdjustment*)` AND refreshed on each
   `QSlider::sliderPressed` (+ `QDoubleSpinBox::editingFinished` for
   keyboard edits). Each `valueChanged` pushes into the layer params
   and emits `previewChanged()` → `canvas_->requestRecomposite()`. On
   `QSlider::sliderReleased`: if current params differ from
   `paramsBefore_`, emit `commitRequested(before, after)` →
   `MainWindow` pushes one `LayerParamsCommand<LevelsAdjustment,
   std::array<LevelsParams, 4>>`. Update `paramsBefore_ = after` so
   the next drag's `before` is correct. Channel-combo + spin-box
   discrete edits: snapshot in interaction guard, commit immediately
   after if changed.
4. **`src/app/MainWindow.{h,cpp}` refactor** — new `propertiesDock_`
   field, instantiate in `buildDocks()` and `tabifyDockWidget`. Wire
   `LayersPanel::editAdjustmentRequested(layer)` →
   `propertiesDock_->bind(layer)` + `raise()` (Levels branch only this
   step; Curves/H-S/B&C stay on dialogs). Wire
   `LayersPanel::activeLayerChanged` → if active is adjustment,
   `bind(layer)`; else `bind(nullptr)`. Wire pane's `commitRequested`
   to push the right `LayerParamsCommand`. `onLayerAddLevels` no
   longer pops a dialog — insert identity Levels layer, push
   `LayerOpCommand` immediately, `bind(raw)` + raise.
   `onEditAdjustmentRequested` Levels branch: just bind + raise.
5. **Delete `src/ui/LevelsDialog.{h,cpp}`** and remove from
   `CMakeLists.txt`.
6. **`tests/test_properties_pane_levels.cpp`** — bind a Levels layer,
   snapshot taken; programmatically drive a slider press → drag →
   release; assert `commitRequested` fires once with correct before/
   after; release with no change emits nothing; second drag's
   `before` equals previous `after`. Uses the `tuxels_add_qt_test()`
   helper added in S0.

After S2: continue with S3 (Curves port — lift CurveEditor out of
CurvesDialog.cpp, build PropertiesPaneCurves, delete CurvesDialog),
then S4 (H-S + B&C ports — both slider-only), then S5 (verify + tag).

Cold-start verification:

```
cmake --build build && ctest --test-dir build
QT_QPA_PLATFORM=offscreen timeout 2 ./build/tuxels
```

Expect 285 passing across 30 executables at S1.

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state (M4 active, S0 + S1 done).
2. This file — S2 detailed instructions + step queue.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
4. `cat /home/james/.claude/plans/quirky-napping-koala.md` — full M4 plan.
5. `git log --oneline -20` + `git tag --list` — recent commits and
   shipped tags (`v0.0.1-m0`, `v0.1.0-m1`, `v0.2.0-m2`, `v0.3.0-m3`).
6. `cmake --build build && ctest --test-dir build` — confirm green
   tree (285 passing at M4-S1).
