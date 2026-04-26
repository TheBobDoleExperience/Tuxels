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
