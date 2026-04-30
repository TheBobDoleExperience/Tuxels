# Tuxels — Chronological Log

Append-only. Never rewrite history. Dated entries in ISO-8601.

---

## 2026-04-20

### Session start — M0 bootstrap

- SCOPE.md had been written 2026-04-17 (pre-existing). Read in full this session.
- User asked to "get started" with basic Photoshop functionality (layers, masking, brushes, blending), and explicitly asked for a workflow that survives auto-compaction.
- Environment check: Ubuntu 24.04, g++ 13.3.0, no Qt6 or CMake installed system-wide. Ubuntu 24.04 has Qt 6.4.2, CMake 3.28.3, lcms2 2.14, libpng 1.6.43, libtiff 4.5, libwebp 1.3 in apt repos.
- User confirmed: install via `sudo apt`, minimal MVP scope.
- Plan file written and approved: `/home/james/.claude/plans/modular-singing-teacup.md` (Milestone M0, 10 steps S1..S10).
- Persistent memory files saved: `project_tuxels.md` (project overview), `feedback_workflow.md` (doc-update discipline), `MEMORY.md` (index).
- Tasks #1..#10 created in task list, one per step.
- **S1 started**: `.gitignore`, `docs/STATUS.md`, `docs/NEXT.md`, `docs/LOG.md` (this file), `docs/ARCHITECTURE.md`, `docs/BUILD.md` created. `sudo apt install …` running in background.
- **S1 → S4 completed** same day. Four commits on `main`: `02626bc` scaffolding, `81908dc` CMake+Qt hello window, `75d2b69` tiled image buffer, `2d6d7e8` layer tree + compositor + 13 blend modes. 43 unit tests pass (`ctest --test-dir build`).
- **S5 completed**: UI shell wired. `src/core/Document.h` (Qt-free model holding width/height + LayerTree + active index). `src/ui/CanvasView.{h,cpp}` (QWidget — simplified from QOpenGLWidget for M0; checkerboard backdrop, cached QImage composite, Ctrl+wheel zoom, middle/shift+drag pan). `src/ui/LayerRowWidget.{h,cpp}` (per-row visibility/thumb/name/blend-combo/opacity). `src/ui/LayersPanel.{h,cpp}` (QDockWidget with QToolBar + QListWidget; renders top-down). `src/app/MainWindow.{h,cpp}` rewritten: owns Document, hosts CanvasView centrally and LayersPanel on right, Layer menu with add/delete, populates a 3-layer sample doc (white bg / red rect / green Multiply disc). Build error in `MainWindow.cpp:54` (`qApp` used without `<QApplication>` include) — fixed by adding the include. Headless run OK via `-platform offscreen`. Commit `9a0a4d0`.
- **S6 completed**: PNG I/O + File menu wired. `src/io/PngIO.{h,cpp}` uses `QImageReader`/`QImageWriter` through `QImage::Format_RGBA8888` (no sRGB linearization for M0 — passthrough). Extracted into a separate `tuxels_io` STATIC library depending on `Qt6::Gui` so tests can link it without pulling in Widgets or MOC. `File → Open…` wraps the loaded PNG in a fresh `Document` with a single PixelLayer named after the file; `File → Export As PNG…` composes the current document into a `TuxImage` and writes sRGB 8-bit PNG. New `tests/test_png_io.cpp` (round-trip, out-of-range clamp, missing-file error, empty-image error) — 4 tests — now at 47 total passing. Commit `7e0488d`.
- **S7 completed**: Brush tool. `src/brush/RoundBrush.{h,cpp}` — `RoundBrushParams` (diameter, hardness, opacity, flow, spacingRatio, color) with a precomputed row-major kernel; rebuilds only on param change. Soft falloff uses inverse smoothstep between `hardness·radius` and full radius. `src/brush/BrushEngine.{h,cpp}` — state machine for strokes: `beginStroke` lays first stamp, `continueStroke` advances distance-based spacing (carries leftover distance across segments so spacing is continuous around path turns), `endStroke` resets. Per-stamp composite uses straight-alpha "source over surface" (TuxImage is straight-alpha). `src/tools/{ToolBase.h,BrushTool.{h,cpp}}` — polymorphic tool interface; BrushTool instantiates a BrushEngine against the active PixelLayer's image when a stroke begins and dynamic_casts to reject non-pixel active layers. CanvasView wires a `ToolBase*`, forwards left-button mouse events (converted widget→image-space via `(pt - pan)/zoom`), and emits `layerPainted` on stroke end for panel thumbnail refresh. MainWindow owns the BrushTool and binds `[`/`]` shortcuts for diameter ±10%. 7 new tests in `tests/test_brush.cpp` (kernel shape, monotonic soft falloff, spacing rounding, center stamp, stroke coverage, off-canvas clipping, opacity×flow scaling). 54 total tests passing. Commit `4cfcf0a`.
- **S8 completed**: Undo/redo. `TuxImage` gained `beginRecord` / `stopRecord` — a copy-on-write recording window where the first write to each tile during the window swaps in a clone and captures the original shared_ptr as the "before" snapshot. `stopRecord()` returns a `{before, after}` TileSnapshot pair so undo/redo are pointer-swap cheap. `src/history/{Command.h, UndoStack.h, PaintCommand.h, LayerOpCommand.h}` — Command abstract base, 64-deep UndoStack with redo-clear-on-push, PaintCommand stores before/after snapshots of a single TuxImage and swaps them, LayerOpCommand is a lambda-pair wrapper for structural/property changes. `BrushTool` now bookends its stroke with `beginRecord()` / `stopRecord()` and exposes `takeLastStroke()`. `LayerRowWidget` refactored: visibility and blend edits now emit `old→new` signals instead of mutating + emitting `layerMutated`; opacity slider live-previews during drag and emits `opacityEditCommitted` on release (one undo entry per gesture, not per intermediate value). `LayersPanel` re-emits those signals. `MainWindow` wires all of them to LayerOpCommand lambdas, owns a `UndoStack`, and binds `Ctrl+Z` / `Ctrl+Shift+Z`. `setDocument` now clears the undo stack. `tests/test_undo.cpp` — 5 new tests (COW snapshot semantics, PaintCommand pixel restore, brush-stroke round-trip, max-depth pruning, push-clears-redo). 59 total tests passing.
- **S9 completed**: Layer masks in UI. `Document` gained `PaintTarget { Layer, Mask }` + getter/setter (default Layer). `BrushTool::StrokeInfo` grew a `TuxImage* target` field; in `press()` the tool reads `doc.paintTarget()` and, if `Mask` and the active layer has one, routes writes to `active_->mask->image` instead of `active_->image`. MainWindow's `onLayerPainted` now builds the `PaintCommand` against `info.target` so undo/redo lands in the right image. `LayerRowWidget` gained a second 40×40 mask thumbnail (hidden when `layer->mask == nullptr`) built from the mask's red channel as a grayscale preview. An `eventFilter` on both thumbnail labels catches clicks: left-click picks the corresponding `PaintTarget`, shift-click on the mask thumb emits a `maskEnabledToggleRequested(oldVal, newVal)` signal, right-click shows a mini menu with "Delete Mask". Active thumb gets a 2-px cyan border via stylesheet; a disabled mask overlays a semi-transparent red tint. `LayersPanel::refresh` calls `row->setPaintTarget(doc->paintTarget())` for every row so highlighting tracks state. `MainWindow` added menu items `Layer → Add Layer Mask` (white-filled mask sized to doc, switches paint target to Mask, undoable via LayerOpCommand stashing the `unique_ptr<LayerMask>`) and `Layer → Delete Layer Mask`. New signal handlers: `onLayerPaintTargetChange` (activates the clicked layer if not already active, sets paint target, refreshes panel), `onLayerMaskEnabledToggle` (LayerOpCommand bool flip), `onLayerDeleteMaskRequest` (LayerOpCommand stashing the mask). BrushTool simplification: no brush color override when painting a mask — the brush's current color.r becomes the mask intensity, which the compositor already reads. So a black brush paints hide, a white brush paints reveal; users change the brush color explicitly. Still 59 tests passing (no new tests — compositor mask behavior was already covered in test_compositor, and the UI wiring is straightforward).
- **S10 partial (pre-tag user testing)**: user began the DoD walkthrough and flagged two gaps:
  - **No tool/color UI**: the M0 plan listed a Tools dock but S5 shipped only the right-side Layers dock. Brush color was stuck at black (RoundBrushParams default) with no UI knob — item 7 (mask white-reveal) was therefore unverifiable.
  - **Visible paint latency**: every mouse-move triggered a whole-document `compose()` + full float→byte cache upload. For 1024×768 that's ~786K pixels × N layers per tick, which shows up as drag-lag.
  Fixes built in one session:
  - `compose(tree, out, Rect)` overload restricts the tile loop to tiles intersecting a pixel rect. Existing full-compose path kept as the zero-arg overload. 3 new tests in `test_compositor` (covered-tile-only update; empty-rect no-op; tile-boundary crossing).
  - `BrushEngine` tracks an `incremental_` Rect alongside the existing stroke `bounds_`; `takeIncrementalBounds()` drains + resets it. `BrushTool::takeDirtyRect()` forwards via a new `ToolBase` virtual (default returns empty). `CanvasView` now tracks a `dirtyRect_` accumulator and a `requestRecomposite(Rect)` overload that unions into it and passes a widget-space rect to `QWidget::update()`. The paint-event path picks full vs partial based on whether the dirty rect is empty.
  - `src/ui/ToolsPanel.{h,cpp}` added — left QDockWidget with foreground/background color swatches (click → `QColorDialog`, X swap, D reset-to-black/white), plus size / hardness / opacity / flow sliders wired straight into the live `RoundBrush`. A small `Swatch` QFrame subclass (anonymous namespace, no Q_OBJECT → no MOC) uses a `std::function` callback for click dispatch. `MainWindow` instantiates it in `buildDocks()` at `Qt::LeftDockWidgetArea`, keeps it in sync via `refreshFromBrush()` when `[`/`]` nudges the diameter, and registers global X/D `QAction`s that forward to the panel.
  - Tests: 62 total now passing (was 59; three new partial-compose tests). Build warning-clean.
  Still pending before tag: user re-verifies items 7 (white reveal), 8 (thumb-click cyan border), 9 (shift-click red tint), and the latency feel with the new partial-recompose path.

### S10 complete — v0.0.1-m0 tagged

- User re-verified DoD items 7/8/9 and the drag latency after the partial-recompose + ToolsPanel rebuild — all clean.
- Two leftover gaps fixed in the same session:
  - **Ctrl+Z felt laggy** because `refreshAfterUndoRedo` always full-recomposed. Added `virtual Rect Command::dirtyRect()`; `PaintCommand` computes the bounding rect of all tile coords in its `before_` map; `UndoStack::undo/redo` now return a `UndoResult { touched, dirtyRect }`; `MainWindow` routes that into a partial recomposite when non-empty. LayerOp undos still full-recompose (they return empty rect) since the dirty region isn't cheaply computable.
  - **Shift-click mask "red tint" was invisible** — stylesheet `background-color` on a QLabel is masked by any opaque pixmap. Fixed by baking the tint into the mask thumbnail pixmap itself (0.45-alpha blend toward r=220 when `!mask->enabled`); stylesheet now only carries the active-thumb border.
- **Brush cursor outline** added per user request: `ToolBase::cursorRadiusPx()` virtual (default `nullopt`); `BrushTool` returns `diameter/2`. `CanvasView` tracks `cursorWidgetPos_` + `cursorInCanvas_`, overrides `enterEvent`/`leaveEvent`, and in `mouseMoveEvent` calls `moveBrushCursorTo()` which partial-invalidates the old + new ring rects. `paintEvent` draws concentric black+white 1-px ellipses so the ring reads against any background. `refreshBrushCursor()` is called from the `[`/`]` handlers so the ring resizes live.
- Tag: `git tag v0.0.1-m0` on the resulting HEAD. 62 unit tests passing (3 new `partial_compose_*` tests from the S10-partial session; brush cursor ring is UI-only, no new unit tests).
- STATUS.md updated: S10 → ✅, M0 → shipped. NEXT.md rewritten for M1 planning.

## 2026-04-26

### M4-S0 — ToolsPanel accordion shell

- Approved M4 plan: `/home/james/.claude/plans/quirky-napping-koala.md`
  ("Adjustment Polish + Tools UX" — 6 steps S0–S5, target tag
  `v0.4.0-m4`). Three tracks: ToolsPanel accordion (S0), clip-to-layer
  flag (S1), Properties dock + per-adjustment ports (S2–S4).
- Settings: switched `~/.claude/settings.json` to
  `permissions.defaultMode = "bypassPermissions"` so the build/test
  loop doesn't prompt; denylist still blocks `git push --force`,
  `git reset --hard`, `git clean -f`, `git branch -D`, `rm -rf` of
  root/home.
- New `src/ui/CollapsibleSection.{h,cpp}` — Q_OBJECT widget with a
  clickable header (chevron + name + shortcut hint) above an arbitrary
  body. Two distinct gestures: header click emits `headerClicked`
  (panel maps to "activate this tool"); chevron click toggles
  expansion + emits `chevronClicked`. Header NEVER touches expansion
  (the user's manual collapse choices stick). EventFilter installed on
  both header and chevron — chevron's filter consumes its event so the
  header branch never sees a chevron click. Test hooks
  `simulateHeaderClick` / `simulateChevronClick` drive the gestures
  programmatically without synthesizing Qt mouse events.
- `src/ui/ToolsPanel.{h,cpp}` rewritten — ~760 lines of picker-row +
  `setVisible(true/false)` group toggling replaced by 10
  `CollapsibleSection`s in a vertical accordion: Move (V), Marquee
  (M), Lasso (L), PolyLasso (P), Wand (W), SelectByColor (⇧W), Crop
  (C), Brush (B), Bucket (G), Free Transform (⌃T). FG/BG color
  swatches stay pinned at top. The accordion is wrapped in a
  `QScrollArea` (widgetResizable) so a fully-expanded state stays
  navigable in a short dock. `setActiveTool` walks the section map
  and toggles only the highlight — never touches expansion. Wand and
  SBC have separate body widgets (each section is self-contained) but
  share state: `onWandToleranceChanged` mirrors the slider value into
  both rows; `setWandMode` reflects into both sets of combine-mode
  buttons. Same pattern for Lasso ↔ PolyLasso. Tools without options
  (Move/Crop/Transform) get a one-line tip QLabel so every section
  has uniform visual weight.
- Public surface preserved verbatim — `setBrushTool`, `setBucketTool`,
  `setMagicWandTool`, `setSelectByColorTool`, `setLassoTools`,
  `setActiveTool`, `setMarqueeMode`, `setWandMode`, `setLassoMode`,
  `refreshFromBrush`, signals `toolPicked` / `marqueeModeChanged` /
  `wandModeChanged` / `lassoModeChanged` — so MainWindow needed zero
  changes. New test-only accessor `ToolsPanel::sectionFor(ToolId)`
  returns the section for inspection in tests.
- New CMake helper `tuxels_add_qt_test()` adds Qt-widget tests with
  AUTOMOC + Qt6::Widgets + `QT_QPA_PLATFORM=offscreen`. Used for
  `test_collapsible_section` (7 cases: default state, idempotent
  setExpanded, setActive flag, header click leaves expansion alone,
  chevron toggles + emits, setBody installs/replaces) and
  `test_tools_panel` (8 cases: all 10 tools have sections, default
  active is Brush, setActiveTool highlights only one section,
  setActiveTool does not change expansion, header click emits
  toolPicked with correct ToolId, the three mode setters are silent
  reflectors).
- 277 tests passing across 29 executables (was 275/27 at v0.3.0-m3).
  Build clean. Headless smoke test (`QT_QPA_PLATFORM=offscreen ./build/tuxels`)
  starts the app cleanly.

### M4-S1 — Adjustment-layer clip-to-layer flag

- `LayerBase::clipToBelow` bool field added (on the base, not just
  `AdjustmentLayer`, so future PixelLayer clipping can reuse it
  without surgery — M4 only honors it for adjustment layers).
- `compositor/compose.cpp::composeTileRange` carries a per-tile
  `lastBaseAlpha[kTilePixels]` buffer + `hasBase` flag. Every
  non-clipped pixel layer's source alpha (pre-blend) is captured
  into the buffer; clipped adjustments multiply the per-pixel lerp
  factor `f *= lastBaseAlpha[idx]`. Adjustment layers (clipped or
  unclipped) do NOT update `lastBaseAlpha` — only pixel layers
  reset the base. Clipped adjustment with no preceding pixel
  layer is skipped entirely (matches PS).
- `.txl` bumped to v4 — new `ClipToBelow : uint8` byte after
  `HasMask` and before `Opacity` in the layer record. v1/v2/v3
  readers preserved (load with `clipToBelow == false`); writer
  always emits v4. Format-comment header updated.
- `LayerRowWidget` now shows a `↳` glyph + tooltip when the bound
  layer has `clipToBelow == true`, hidden otherwise. New
  `contextMenuEvent` override offers "Create Clipping Mask" /
  "Release Clipping Mask" on the row body. Emits a new
  `toggleClipToBelowRequested(LayerBase*)` signal.
- `MainWindow::onToggleClipToBelow` (Ctrl+Alt+G — PS default) and
  `onLayerToggleClipToBelow(LayerBase*)` (right-click route from
  LayerRowWidget): both flip `clipToBelow` and push a
  `LayerOpCommand` (id-keyed lookup so undo/redo survives reorders),
  label `"Create Clipping Mask"` / `"Release Clipping Mask"` based
  on direction. No-op when active is the bottom layer (no base);
  status bar shows a hint. Menu item under `Layer → Create Clipping
  Mask…`.
- 285 tests across 30 executables (was 277/29). 5 new cases in
  `test_clip_to_below`: unclipped affects full doc, clipped gated
  by base alpha, two adjacent clipped share one base, clipped-with-
  no-base no-op, unclipped adjustment doesn't reset the base.
  2 new cases in `test_txl_io`: v4 round-trip of mixed
  clipped/unclipped layers, hand-crafted v3 file loads with
  `clipToBelow == false`. Headless smoke test green.

### M4-S2 — Properties dock + Levels port

- New `src/ui/PropertiesDock.{h,cpp}` — `QDockWidget` parked on
  `Qt::RightDockWidgetArea`, default tab-stacked under
  `LayersPanel` via `tabifyDockWidget(layersPanel_,
  propertiesDock_)`. Owns a `QStackedWidget` with two pages: an
  empty-state QLabel ("Select an adjustment layer to edit its
  properties.") and the Levels pane. API: `bindLevels(layer,
  hist)` and `bindNothing()`. Re-emits `previewChanged` and
  `levelsCommitRequested(layer, before, after)`.
- New `src/ui/PropertiesPaneLevels.{h,cpp}` — bare `QWidget`
  (Q_OBJECT) with the channel combo + 5 slider+spinbox pairs +
  histogram backdrop, lifted from the deleted `LevelsDialog`. The
  histogram view (`LevelsHistogramView`) is also lifted into the
  pane's .cpp as a private class. **Snapshot/commit discipline:**
  `paramsBefore_` snapshot taken on `bind()` AND on each
  `QSlider::sliderPressed`; `valueChanged` mutates the layer +
  emits `previewChanged`; `sliderReleased` checks for diff and
  emits `commitRequested(layer, before, after)` if anything
  changed, then re-snapshots so the next drag's `before` is
  correct. Spin-box keyboard edits use `editingFinished` for the
  same diff/commit. Channel combo switch reloads widgets +
  re-snapshots (no commit emitted).
- Deleted `src/ui/LevelsDialog.{h,cpp}` (and CMakeLists entry).
- MainWindow refactor: new `propertiesDock_` field instantiated in
  `buildDocks()`. `LayersPanel::editAdjustmentRequested(levels)`
  now binds the dock + raises (no modal); the existing modal
  branches for Curves/H-S/B&C stay until S3/S4. New helper
  `bindActiveAdjustmentToDock()` dispatches on layer kind
  (Levels → bind; non-adjustment or kind not yet ported → empty
  state). Wired to `LayersPanel::activeLayerChanged` so the
  active-layer selection in the panel updates the dock.
  `onLayerAddLevels` no longer pops a dialog — inserts identity
  layer, pushes `LayerOpCommand` immediately, binds dock + raises.
  The "Cancel removes the layer" semantic from M3 becomes "Ctrl+Z
  removes the layer" (non-modal default).
- New helper `MainWindow::histogramBelow(LayerBase*)` extracted
  from the visibility-stash pattern that was inlined in the
  pre-S2 `onEditAdjustmentRequested`. Other adjustment branches
  (Curves/H-S/B&C) still inline the same pattern; will refactor
  to use the helper when their dock panes land in S3/S4.
- 292 tests across 31 executables (was 285/30). 7 new cases in
  `test_properties_pane_levels`: default state unbound, bind takes
  snapshot, drag/release emits one commit (multiple in-drag value
  changes still = one commit), release without change emits
  nothing, second drag's `before` equals first drag's `after`,
  unbind clears layer pointer, previewChanged fires during drag.
  Headless smoke test green.

### M4-S3 — Curves port

- New `src/ui/CurveEditor.{h,cpp}` — Q_OBJECT widget lifted out
  of `CurvesDialog.cpp`. Two-signal lifecycle: `previewChanged`
  on every visible mutation; `interactionEnded` once per
  commit-worthy gesture (mouse release after drag/add; right-click
  after successful remove). Editor carries
  `pendingInteractionCommit_` so a left-press-no-drag-then-release
  on existing point doesn't fire interactionEnded. Programmatic
  `setPoints` / `setHistogram` / `setChannel` do NOT emit signals
  (pane uses them on bind / channel-switch).
- New `src/ui/PropertiesPaneCurves.{h,cpp}` — mirrors
  PropertiesPaneLevels shape: channel combo + CurveEditor +
  histogram backdrop. Snapshot/commit discipline: bind snapshots
  `paramsBefore_` (PointsArray, all 4 channels) + loads active
  channel into editor. `onEditorPreviewChanged` mutates layer
  points for active channel + re-emits previewChanged.
  `onEditorInteractionEnded` diffs vs paramsBefore_, emits
  `commitRequested(layer, before, after)` if changed, re-snapshots.
  Channel switch reloads editor + re-snapshots (no commit).
- `PropertiesDock` extended with `bindCurves(layer, hist)` +
  `curvesCommitRequested` re-emit. New Curves pane added to the
  internal QStackedWidget.
- MainWindow: `onLayerAddCurves` mirrors `onLayerAddLevels`
  line-for-line (insert identity + push LayerOpCommand + bind dock
  + raise; Ctrl+Z removes the layer). `onEditAdjustmentRequested`
  Curves branch becomes 2 lines (bind dock + raise).
  `bindActiveAdjustmentToDock` extended with CurvesAdjustment
  dispatch. New curvesCommitRequested lambda wraps the commit in
  `LayerParamsCommand<CurvesAdjustment, CurvesAdjustment::PointsArray>`.
- Deleted `src/ui/CurvesDialog.{h,cpp}` + CMakeLists entry.
- 299 tests across 32 executables (was 292/31). 7 new cases in
  `test_properties_pane_curves`: default unbound, bind takes
  snapshot, drag/release emits one commit (mid-drag preview
  doesn't), release without change emits nothing, second drag's
  `before` equals first drag's `after`, channel switch doesn't
  commit, unbind clears. Headless smoke test green.

### M4-S4 — Hue/Sat + Brightness/Contrast ports

- New `src/ui/PropertiesPaneHueSat.{h,cpp}` — three slider+spinbox
  pairs (hue ∈ [-180, 180]°, saturation ∈ [-100, 100]%, lightness
  ∈ [-100, 100]%) on the same press-snapshot / release-commit
  discipline as Levels/Curves panes. Sat/light divided by 100 for
  the float param ∈ [-1, 1]. No histogram.
- New `src/ui/PropertiesPaneBrightnessContrast.{h,cpp}` — two
  slider+spinbox pairs (brightness, contrast both ∈ [-100, 100]%)
  divided by 100 for the float param. No histogram.
- `PropertiesDock` extended with two new pages: `bindHueSat(layer)`
  + `bindBrightnessContrast(layer)`, plus `hueSatCommitRequested` /
  `brightnessContrastCommitRequested` re-emits.
- MainWindow: `onLayerAddHueSaturation` and
  `onLayerAddBrightnessContrast` mirror the Levels/Curves pattern
  (insert identity + push LayerOpCommand + bind dock + raise;
  Ctrl+Z removes). `onEditAdjustmentRequested` H-S and B&C
  branches become 2 lines each. `bindActiveAdjustmentToDock`
  extended for both. Two new commit lambdas in `buildDocks()`.
  `onEditAdjustmentRequested` also refactored to use the new
  `histogramBelow()` helper instead of inline visibility-stash
  (now no longer dead code post-port).
- Deleted `src/ui/HueSatDialog.{h,cpp}` and
  `src/ui/BrightnessContrastDialog.{h,cpp}` + CMakeLists entries.
- 307 tests across 33 executables (was 299/32). 8 new cases in
  `test_properties_pane_smaller` (4 per pane): default unbound,
  drag/release emits one commit, release without change emits
  nothing, unbind clears pointer. Headless smoke test green.

### M4-S0 through S4 complete; ready for user DoD walkthrough (S5)

- All M3 modal dialogs are gone. Levels/Curves/H-S/B&C are now
  edited live in the Properties dock parked under LayersPanel on
  the right side, with one undo entry per drag-end.
- ToolsPanel is the new accordion of named, collapsible per-tool
  sections; FG/BG swatches pinned at top.
- Adjustment layers carry a `clipToBelow` flag (Ctrl+Alt+G or
  right-click in the Layers panel) — adjustment effect gated by
  the alpha of the immediate non-clipped pixel layer below.
- 5 commits on `main` between `e6a86e1` and now, all building
  cleanly with 100% test pass each step.
- Pending: S5 DoD walkthrough (user-driven) → tag `v0.4.0-m4`.

### M4-S5 — v0.4.0-m4 tagged

- User confirmed DoD walkthrough end-to-end: ToolsPanel accordion
  (chevron-only collapse, header-click activates, FG/BG swatches
  pinned), clip-to-layer (Ctrl+Alt+G + right-click + `↳` glyph +
  `.txl` v4 round-trip), Properties dock (binds for all four
  adjustment kinds, live preview during drag, exactly one undo
  entry per drag-end, empty state when active layer isn't an
  adjustment), regression-clean (M3 keyboard shortcuts unchanged,
  M3 `.txl` v3 files load with `clipToBelow == false`).
- Tagged `v0.4.0-m4` on the `1d776f4` HEAD (annotated tag with
  highlight body, matching the M0/M1/M2 convention).
- 307 unit tests passing across 33 executables. `STATUS.md`
  flipped to "M4 ✅ shipped"; `NEXT.md` rewritten for M5 kickoff
  (smart objects / PSD import / layer groups as candidates).

## M5 — Layer Groups

Plan: `/home/james/.claude/plans/let-s-make-a-plan-resilient-engelbart.md`.

### M5-S0 — Tree refactor + GroupLayer skeleton + active-id migration

- New `src/layers/GroupLayer.{h,cpp}`: `GroupLayer : public
  LayerBase`, owns recursive `std::vector<std::unique_ptr<LayerBase>>
  children`, default `BlendMode::PassThrough`, `bool isExpanded =
  true` for UI state, `kind() final → Group`, `renderTile() final
  → false`.
- `LayerKind::Group` added to enum. `BlendMode::PassThrough`
  appended at the end (numeric ordinals 0..12 stay stable for v4
  back-compat).
- `LayerTree` extended with recursive helpers: `findById`,
  `locate(id) → {parent, index}`, `flatten()` (depth-first
  child-then-self so bottom-up consumers see children before the
  group node), `forEach(fn)`, `insertAtPath`, `removeFromPath`,
  cross-parent `move`. All in new `src/layers/LayerTree.cpp`
  (need `GroupLayer` concretely).
- `Document` migrated from `int activeLayerIndex_` to
  `LayerId activeLayerId_`. `activeLayer()` resolves via
  `tree_.findById`. Legacy `activeLayerIndex()` getter/setter
  remain as transitional shims that walk root-flat positions.
- Compose's flat per-tile loop replaced with
  `for (LayerBase* l : tree.flatten())`; group nodes are
  `continue`d (S0 stub — S1 turns on real recursion).
- `MoveLayerCommand`, `TransformCommand` route through
  `tree.findById`. `CropCommand::Snapshot` becomes
  `unordered_map<LayerId, LayerEntry>` so reorder between capture
  and undo routes pixels back to the right layer by id (was
  index-keyed, latent bug).
- `MainWindow` migrated through ~25 sites: `prevActive →
  prevActiveId`, `setActiveLayerIndex(idx) → setActiveLayerId
  (layer->id)`, the inline `findById` lambda in
  `onLayerToggleClipToBelow` replaced by `doc_->tree().findById`.
  `histogramBelow` walks `tree.flatten()`.
- 9 new `test_layer_tree` cases. 1 new `test_crop` case
  (id-keyed snapshot survives reorder).
- 317 tests across 34 executables. Commit `57b07d4`.

### M5-S1 — Compose recursion: Pass-Through + isolated branches

- `compose.cpp` rewritten with recursive
  `composeChildren(ctx, layers, accum, lastBaseAlpha, hasBase)`
  helper plus `composeGroupPassThrough` and `composeGroupIsolated`.
- Pass-Through (the GroupLayer default): hot-path inlines the
  trivial case (`opacity==1`, no mask, no clip) bit-exact with
  flat layer list. Otherwise snapshots `accum` into `accumBefore`,
  recurses, and per-pixel lerps
  `accum = mix(accumBefore, accum, opacity * maskV)` (with
  `f *= lastBaseAlpha[idx]` if `clipToBelow`).
- Isolated (any non-Pass-Through blend): allocates `accum2` +
  scope-local `lastBaseAlpha2` + `hasBase2 = false`, recurses,
  then back-composites `accum2` into the parent `accum` via
  `compositePixel(parentAccum, src, group->blend, f, ...)` with
  `f = opacity * maskV * (clipToBelow ? lastBaseAlpha[idx] : 1)`.
  Updates parent's `lastBaseAlpha` from `accum2`'s alpha so a
  clipped adjustment immediately above gates against the group's
  contribution.
- Clipped group with no base = no-op. Defensive
  `case PassThrough: → bm_normal` in `applyBlend` switch.
- Buffers (`layerTile`, `adjScratch`) shared via a `Ctx` struct +
  raw pointers — single allocation per outer `composeTileRange`.
- 12 new `test_compose_groups` cases: Pass-Through equals flat
  (bit-exact), PT+adjustment leaks out, isolated+adjustment
  doesn't leak, isolated opacity multiplies, group mask gates,
  nested PT inside isolated, clipped group no-base no-op, clipped
  group gates by base alpha, clipped adjustment inside isolated
  gates against group-local base only, empty group no-op,
  invisible group skips recursion, PT with opacity=0.5 partial-leak.
- 341 tests across 35 executables. Commit `4692f95`.

### M5-S2 — `.txl` v5 with PSD-style group section dividers

- `kVersionCurrent = 5`. New constants `kVersionV4 = 4`,
  `kLayerKindOpenGroup = 10`, `kLayerKindCloseGroup = 11`.
- Writer refactored into recursive `writeLayerRecords` helper.
  Pixel + adjustment kinds emit a single record. Groups emit
  OpenGroup record (header + `NumImageTiles=0` sentinel + 1-byte
  `IsExpanded` descriptor + mask block) → recurse children →
  CloseGroup record (header-only marker, all-zero fields,
  `NameLen=0`, no payload after the header).
- New `countRecords` / `countRecordsList` helpers compute on-disk
  record count for `NumLayers` (groups contribute 2). Documented
  the semantic shift in `TxlIO.h`.
- Reader maintains `std::vector<GroupLayer*> groupStack` (empty =
  root); CloseGroup detected immediately after the standard
  header (validates version + non-empty stack + `nameLen==0`,
  pops, continues). OpenGroup constructs `GroupLayer`, reads
  descriptor + mask, attaches to current parent + pushes onto
  stack. Existing kinds attach via shared `attach` lambda.
- v1/v2/v3/v4 readers preserved with explicit version constants
  + four gating flags. EOF with non-empty stack and CloseGroup
  with empty stack are both errors.
- Group masks are doc-sized (extends the existing pixel-vs-non-
  pixel mask-dim branch).
- 6 new test_txl_io cases: empty group; group with three pixel
  children; three-deep nested groups; group attributes (mask +
  clipToBelow=true + Multiply blend + opacity=0.7 + isExpanded=
  false + visible) survive intact; mixed-root layout
  `[pixelA, group{[Levels, pixelB]}, pixelC]` preserves nesting +
  ids + subclass dispatch; hand-crafted v4 file loads cleanly.
- 347 tests across 35 executables. Commit `19a4898`.

### M5-S3 — LayersPanel: indented rows + chevron + New Group menu

- `LayerRowWidget` extended for groups:
  - `chevron_` QLabel between visibility checkbox and clip glyph.
    Visible only when bound to a `GroupLayer*`. Click emits new
    `chevronToggled(GroupLayer*)` signal and is swallowed before
    bubbling to the QListWidgetItem.
  - Folder-glyph thumb via `QStyle::SP_DirIcon` over a tinted
    background.
  - `setIndentDepth(int)` updates the layout's left content margin
    to `4 + depth*12`.
  - Two blend lists: `kPixelBlendList` (13, no Pass-Through) and
    `kGroupBlendList` (14, Pass-Through first); swapped on bind
    via `rebuildBlendCombo(includePassThrough)`.
- `LayersPanel` rebuild rewritten: new `buildDisplayList(children,
  depth, out)` recursive helper walks the tree top-down (PS reading
  order, children iterated in reverse so the topmost child appears
  just below its group header), recurses into expanded groups at
  `depth+1`, skips children of collapsed groups (compose still
  composites them — collapse is purely UI).
- `MainWindow::onLayerNewGroup` slot wraps `GroupLayer` creation
  in `LayerOpCommand` mirroring `onLayerAdd`'s shape; group named
  "Group N" via `tree.forEach`. `Layer → New Group` menu entry.
  Disabled placeholders for `Layer → Group Layer` (Ctrl+G) and
  `Layer → Ungroup Layer` (Ctrl+Shift+G).
- 6 new `test_layers_panel_groups` cases (Qt-widget): indented
  top-down rendering with depths [0,0,1,1,0]; chevron collapse
  hides children + flips `isExpanded`; active highlight on group
  row; group blend combo has 14 items + Pass-Through, pixel rows
  have 13 + no Pass-Through; chevron only visible for group rows;
  collapsed-group walker skips all descendants.
- 353 tests across 36 executables. Commit `3929f2c`.

### M5-S4 — Group / Ungroup commands + scope-local Up/Down

- `Layer → Group Layer` (Ctrl+G) wraps the active layer in a new
  group inserted at the active's parent + index. doIt: capture
  `parentId` + `idx` via `tree.locate`, pluck via `removeFromPath`,
  build a fresh `GroupLayer` (or reuse a stashed one for redo)
  with the layer as `children[0]`, `insertAtPath` + set active.
  undoIt: extract layer back from group, remove group, reinstall
  layer at original parent+idx, stash group for redo.
- `Layer → Ungroup Layer` (Ctrl+Shift+G) is enabled only when
  active is a Group; promotes children into the parent at the
  group's old index in stored bottom-up order. Active = bottom-
  most ex-child. Empty-group fallback: parent[oldIdx] →
  parent.last → parent → 0. undoIt re-collects children by id and
  rebuilds the stashed group, preserving the group's id.
- `onLayerNewGroup` refined: inserts at active's parent +
  (active's index + 1) so the new group appears immediately above
  the active layer.
- `onLayerMoveUp/Down` rewritten to be scope-local — uses
  `tree.locate(activeId)` for parent + idx; top/bottom of group
  → no-op + status-bar message.
- Menu enable/disable via new `updateGroupActionStates()` helper,
  called from `onActiveLayerChanged()` and the Layer menu's
  `aboutToShow`. Keyboard shortcuts honor the QAction enable bit.
- 9 new `test_group_commands` cases (non-Qt; replicates closure
  logic at the Document level): wraps in group at same idx; undo
  restores original tree + active; ungroup promotes children in
  stored order; ungroup undo rebuilds with same group id +
  children; empty-group ungroup just deletes; group → ungroup →
  undo → undo loop restores initial state; up/down inside a group
  is scope-local; New Group inserts at (active+1) for both root
  and inside-group cases.
- 362 tests across 37 executables. Commit `d20a801`.

### M5-S5 — Group-aware delete + dock + addAdjustment routing

- `onLayerDelete` switched from `tree.removeAt(idx)` (root-only,
  index-based) to `tree.locate(activeId)` + `removeFromPath`, so
  deleting a layer inside a group works cleanly. The unique_ptr
  ownership chain transitively destroys the children when a group
  is the deleted layer; the closure stash holds the entire
  subtree so undo `insertAtPath`s everything back.
- `bindActiveAdjustmentToDock` gains an explicit `LayerKind::Group`
  arm that calls `bindNothing()` — was implicit fallthrough; now
  signposted as a hook for the future M6 group-properties pane.
- The four `onLayerAdd*` slots route through new
  `computeAdjustmentInsertSlot(activeId)` helper:
  - Active is a Group → adjustment lands inside the group at the
    top of its children.
  - Active is a regular layer → adjustment lands at active's
    parent + (active+1).
  - No active → root append (old behavior).
- **Bug fix in `histogramBelow`:** previously hid every layer
  at-or-after target in flatten order, *including ancestor
  groups*. Hiding a parent group skipped the recursion in compose,
  dropping the group's earlier children from the preview composite
  even though they should compose normally below the target. Fix:
  walk the parent chain via `tree.locate` to build a set of
  target's ancestor groups; keep them visible while hiding
  everything else past target.
- 2 new `test_group_commands` cases:
  `delete_active_group_with_children_round_trip`;
  `histogram_below_target_inside_passthrough_group`.
- 364 tests across 37 executables. Commit `276ef98`.

### M5 — substantively complete; ready for user DoD walkthrough (S6)

- Layer groups end-to-end: model (recursive LayerTree +
  GroupLayer), compose (Pass-Through and isolated branches),
  `.txl` v5 round-trip with PSD-style section dividers, panel
  UI (chevron + indent + Pass-Through combo only on groups),
  commands (Ctrl+G / Ctrl+Shift+G / scope-local Up/Down +
  context-aware addAdjustment + delete-group with transitive
  children).
- v1–v4 `.txl` files still load (without groups). Pre-M5 docs
  unchanged structurally.
- 6 commits on `main` between `57b07d4` (S0) and `276ef98` (S5),
  all building cleanly with 100% test pass each step.
- Pending: S6 DoD walkthrough (user-driven) → tag `v0.5.0-m5`.

---

## 2026-04-30

### M6 — UX Polish (Group Properties / D&D / Multi-select) ✅ shipped

User stepped away with explicit "knockout as many features as you
can, push as you go" mandate. M5 tag (`v0.5.0-m5`) had been created
locally on 2026-04-26 but not pushed; preflight confirmed 37/37
green at HEAD, then pushed `origin main v0.5.0-m5`. M6 plan
written to `/home/james/.claude/plans/lets-take-a-look-iterative-platypus.md`
picking the three smallest of the six M5-deferred candidates.

- **M6-S0 PropertiesPaneGroup** (`1e337e3`). New
  `src/ui/PropertiesPaneGroup.{h,cpp}` mirroring
  `PropertiesPaneHueSat`'s snapshot/commit shape. Closes the
  explicit `bindNothing()` TODO in
  `MainWindow::bindActiveAdjustmentToDock`'s Group arm. New
  `GroupProperties { name, blend, opacity, clipToBelow }` struct;
  `LayerParamsCommand<GroupLayer, GroupProperties>` with a 4-field
  atomic setter. `PropertiesDock` extended with `bindGroup` + page
  5 + `groupCommitRequested`. Build hit a Qt6 `qmetatype.h`
  incomplete-type error from forward-declaring `GroupLayer` in the
  dock header (signal payloads need full types); fixed by including
  `layers/GroupLayer.h`. 8 cases in new `test_properties_pane_group`.
  372/38.

- **M6-S1 LayersPanel D&D + cross-parent move** (`c0bddc8`). Item
  widgets (`LayerRowWidget`) eat the mouse events that
  `QListWidget`'s built-in drag detection would need, so a private
  `LayerListWidget : QListWidget` subclass tracks press position
  and initiates a manual `QDrag` once the cursor crosses
  `QApplication::startDragDistance()`. Custom MIME
  `application/x-tuxels-layerid` carrying an 8-byte LayerId. 3-zone
  hit-test inside target row (top quarter = Above, bottom quarter
  = Below, middle half = On). Tree-coordinate translation accounts
  for the panel reversing tree order; same-parent symmetric undo
  via `tree.move(toP, toI, fromP, fromI)`. Cycle detection walks
  ancestor chain. Did NOT subclass `MoveLayerCommand` (which is
  for origin x/y, not tree position) — used `LayerOpCommand`
  closures throughout. 7 cases in new `test_layer_dnd`. 379/39.

- **M6-S2 Multi-select** (`fa5c047`). `Document::selectedLayerIds_`
  + `selectedLayers()`. `LayersPanel` switches to
  `ExtendedSelection`; `itemSelectionChanged` syncs into Document.
  Three batch ops in MainWindow (delete / group / visibility),
  each producing a single undo entry. Filter-descendants pass
  excludes children of selected ancestors so group deletion sweeps
  the subtree via the unique_ptr chain. For batch group, walks
  `tree.flatten()` to collect filtered ids in bottom-up tree
  order so children inside the new group preserve original
  ordering. Tools stay single-active explicitly — multi-active
  tool semantics (bbox-union Move / Transform / Free Transform)
  scoped out for M7. 8 cases in new `test_multi_select`. 387/40.

- **M6-S3** (this commit). STATUS / NEXT / ARCHITECTURE / LOG
  updated. Tag `v0.6.0-m6` + push.

Discovered during M6: M5-S0's `LayerTree::move(fromP, fromI, toP,
toI)` semantics — toIdx is in the post-erase frame for same-parent
moves, which (counterintuitively) ends up being equivalent to "the
final desired position" in the new layout. `vector::insert(begin()+K, val)`
inserts BEFORE existing K, so erase-K-then-insert-at-K returns the
moved item to a position that, when N stays constant, equals the
caller's intended final idx. This is documented in LayerTree.h's
move comment but the equivalence is non-obvious; M6 D&D math relies
on it.

## 2026-04-30 (continued)

### M7 — Multi-select tools polish + Layer Duplicate ✅ shipped

User went out for the day with explicit "knockout as many features
as you can, push as you go" mandate again. M7 ran open-ended (no
plan file) — picked 7 incremental UX wins building on M6's
multi-select substrate:

- **M7-S0** (`25d4137`). MoveTool acts on multi-select. API change:
  `takeCommit()` returning `optional<PendingMove>` →
  `takeCommits()` returning `vector<PendingMove>`. MainWindow's
  dispatch: size==1 → `MoveLayerCommand` (preserves the readable
  status-bar label); size>1 → `LayerOpCommand` whose closures
  iterate over the move list. Non-pixel layers in selection
  (groups, adjustments) skipped silently — they have no origin to
  shift. 3 new test cases.

- **M7-S1** (`0bb1650`). Up/Down at top/bottom of group pop the
  layer OUT into the parent scope. Cross-scope moves use
  `tree.move(fromP, fromI, toP, toI)` with symmetric undo
  `tree.move(toP, toI, fromP, fromI)`. 2 new test cases.

- **M7-S2** (`74dca08`). Custom drop indicator overlay in
  `LayerListWidget::paintEvent`. The 3-zone hit-test factored into
  a private `zoneAt()` so dragMove + drop share one source of
  truth. Group + middle = blue tinted fill + 2px border ("drop
  INTO group"); Above / Below = thin green line at top / bottom
  edge.

- **M7-S3** (`30293be`). ToolsPanel section expanded/collapsed
  state survives app restart via QSettings under
  `ToolsPanel/Section/<toolKey>`. Save-on-toggle. Test isolates
  QSettings to a QTemporaryDir.

- **M7-S4** (`e95bcc7`). Ctrl+J Duplicate Layer for all kinds. New
  `src/layers/CloneLayer.{h,cpp}`. Bug discovered during testing:
  the default `TuxImage` copy ctor shares `shared_ptr<Tile>`s
  rather than deep-copying — writes on a clone perturb the source
  outside a `beginRecord` window. Fixed via `deepCopyImage` helper
  that walks tiles and calls `tile->clone()` each. 6 new test
  cases covering all four adjustment kinds + pixel COW
  independence + group recursive clone.

- **M7-S5** (`20320d5`). Double-click the name label in
  LayerRowWidget → in-place QLineEdit. `editingFinished` commits
  via `LayerOpCommand`; Escape reverts. `bindToLayer` drops
  stale edit state across panel rebuilds. Pixel + adjustment
  layers gain a rename path for the first time.

- **M7-S6** (`7fdb460`). Right-click on layer rows opens a context
  menu: Duplicate / Delete / Rename / Group (non-groups) / Add
  Mask (pixel without existing mask) / Clip-to-below toggle. PS-
  style: right-click activates the row + replaces multi-selection
  with `{layer}`. Rename uses new `LayerRowWidget::beginRename()`
  public helper.

- **M7-S7** (this commit). STATUS / NEXT / ARCHITECTURE / LOG
  updated. Tag `v0.7.0-m7` + push.

Discovered during M7:
- `TuxImage`'s tile-COW is gated on `beginRecord`/`stopRecord`
  windows. Plain copy-construct shares `shared_ptr<Tile>`s. Documented
  the implication for clone use cases in CloneLayer.cpp + STATUS
  + ARCHITECTURE §22.
- Item widgets in QListWidget eat the mouse events QListWidget's
  built-in drag detection would need (re-confirmed from M6-S1).
  `LayerListWidget::mousePressEvent` records the press position +
  `mouseMoveEvent` initiates a manual `QDrag` on
  `QApplication::startDragDistance()`. Worth keeping in mind for any
  other drag-from-row interactions in the future.

Free Transform on multi-select (originally planned as M7-S4)
deferred to M8 — TransformTool's single-source architecture
(one `src_` TuxImage, one `Override`, one PendingCommit) needs a
deep refactor that doesn't fit a polish step.

### M8 — Color labels / Tablet pressure / Rasterize ✅ shipped

Continuing the same autonomous run, M8 picked three more
incremental wins:

- **M8-S0** (`0b7f472`). Per-layer color labels (8-color enum +
  `LayerBase::colorLabel` field). Visual-only — doesn't affect
  compose. LayerRowWidget paints a 4-px stripe at the left edge;
  context menu has a `Color` submenu. `.txl` bumped to v6 with a
  `colorLabel u8` after `clipToBelow`. Pre-v6 readers gated by
  `hasColorLabelByte`. Out-of-range bytes clamp to None to defend
  against corrupt / future-encoding files. New
  `txl_v6_round_trip_preserves_color_label` test.

- **M8-S1** (`2c232ad`). Tablet pressure wired through. CanvasView's
  new `tabletEvent` override pushes pressure into the active tool
  via `ToolBase::setPressure`; BrushEngine's `applyStamp` adds a
  pressure-scale branch (effective diameter + opacity scale by
  pressure). Mouse paths reset pressure to 1.0 so non-tablet input
  keeps producing full-strength strokes. Three new tests including
  a bitwise-identity guard for the pressure==1.0 no-jitter path.

- **M8-S2** (`044deb1`). Rasterize Layer for groups. Clones the
  group's children into a fresh tmp Document at root, composes
  isolated against transparent; the new PixelLayer inherits the
  group's blend + opacity + mask + colorLabel so the composite
  over underlying layers stays consistent. `deepCopyTuxImage`
  promoted out of CloneLayer.cpp's anon namespace into
  CloneLayer.h (PSD import / Merge Down will reuse it).

- **M8-S3** (this commit). STATUS / NEXT / ARCHITECTURE / LOG
  updated. Tag `v0.8.0-m8` + push.

Pace check: M5 → M8 in one autonomous session (4 milestones, 22
commits, 41 executables / 403 cases). Cumulative test growth from
M5 (37 execs / 364 cases) is +4 execs / +39 cases. The big-ticket
"Free Transform on multi-select" item has been deferred from M7
through M8; it's still the right call as a dedicated milestone
because TransformTool's single-source architecture refactor isn't
a polish step.

### M9 — Merge Down ✅ shipped (single-step)

- **M9-S0** (`3f2b586`). PS-style Ctrl+E merges the active pixel
  layer with the pixel layer immediately below. Same
  compose-isolated pattern as M8-S2 Rasterize but with two
  layers as input. Three shared_ptr stashes (below + active +
  merged) ferry the layers across LayerOpCommand cycles. Order-
  sensitive doIt() removal: active first, then below.
- **M9-S1** (this commit). Docs + tag `v0.9.0-m9` + push.

End of session: 5 milestones shipped (M5 push + M6 + M7 + M8 + M9),
24 commits on main, 41 executables / 403 internal cases (up from
M5's 37 / 364). Free Transform on multi-select remains the #1
deferred item; M10 is its natural home.

## 2026-04-30 (continued)

### M10 — Free Transform on multi-select ✅ shipped

Continuing the autonomous run, M10 finally tackles the deferred-
since-M7 architectural item — Free Transform on multi-select.

- **M10-S0** (`25dd3d0`). compose() generalized to accept
  `std::span<const LayerOverride>`. Internal Ctx::override becomes
  a span; per-pixel-layer dispatch uses a new findOverride helper
  (first match wins). Single-pointer overloads kept as thin
  wrappers building 0/1-element spans for backward compat. nullptr
  path = empty span = bitwise identical to pre-M10.

- **M10-S1+S2** (`fad8eff`). TransformTool refactored to multi-
  source. `enter()` collects every PixelLayer from
  `selectedLayerIds()` into a `vector<Source>`; bbox-union of all
  sources' doc rects is the shared transform frame. New
  `buildDocToDoc(centerX, centerY, pivotX, pivotY, sx, sy, angle,
  bboxCx, bboxCy)` is the doc → doc transform applied uniformly.
  `rebuildScratchFor(Source&)` applies it to each source's corners,
  AABBs, allocates per-source scratch, resamples. Legacy `commit()`
  returns optional<PendingCommit> of the first source for the
  existing test surface; new `commits()` returns the full vector.
  `Overlay::layer` mirrors overrides[0]; `Overlay::overrides` is
  the new vector. CanvasView passes the full vector via M10-S0's
  span overloads. MainWindow's commitTransformIfActive dispatches:
  size==1 → single TransformCommand (preserves readable label),
  size>1 → LayerOpCommand iterating over per-source records.

- **M10-S3** (this commit). STATUS / NEXT / ARCHITECTURE / LOG
  updated. Tag `v0.10.0-m10` + push.

End of session: 6 milestones shipped autonomously (M5 push + M6 +
M7 + M8 + M9 + M10), 27 commits on main, 41 executables / 407
internal cases (up from M5's 37 / 364 — that's +4 executables and
+43 internal cases across this session).

Cumulative deferred-list cleared:
  - Multi-select Move (M7-S0) ✓
  - Up/Down crossing groups (M7-S1) ✓
  - Group properties pane (M6-S0) ✓
  - D&D layer reorder + drop-into-group (M6-S1) ✓
  - Multi-select with batch ops (M6-S2) ✓
  - Tablet pressure (M8-S1) ✓
  - Drop indicator polish (M7-S2) ✓
  - ToolsPanel persistence (M7-S3) ✓
  - Layer Duplicate (M7-S4) ✓
  - Rename + context menu (M7-S5/S6) ✓
  - Color labels (M8-S0) ✓
  - Rasterize Layer (M8-S2) ✓
  - Merge Down (M9-S0) ✓
  - Free Transform on multi-select (M10-S1+S2) ✓

Remaining big-ticket items (M11+): PSD import (read-only),
Smart Objects, Layer effects (drop shadow / glow / stroke), Text
layers, Performance pass.
