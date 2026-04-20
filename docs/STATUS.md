# Tuxels — Current Status

**One-paragraph summary:** Milestone **M0 shipped** as `v0.0.1-m0` on 2026-04-20 (commit `be87b70`). **M1 in progress** — S1 (selection model + brush clipping + Select menu), S2 (rectangular marquee + marching ants + persistent combine mode), S3 (paint bucket + scanline flood), S4 (magic wand + floodSelect), and S5 (crop tool + canvas resize) all landed 2026-04-20. 117 unit tests green (13 new in `test_crop`). Plan: `/home/james/.claude/plans/steady-framing-willow.md`.

## Current Milestone: M1 — Selection, Fill, and Native Format

**Definition of Done:** rectangular marquee + marching ants, bucket fill, magic wand, crop, `.txl` native file format round-tripping doc + layers + masks + selection. See plan file for detailed DoD.

### M1 Step Progress

| Step | State | Notes |
|------|-------|-------|
| S1 — SelectionMask + brush clipping + Select menu | ✅ done | 2026-04-20; 75 tests passing (13 new in `test_selection`) |
| S2 — Rectangular marquee + marching ants | ✅ done | 2026-04-20; 86 tests passing (+9 marquee mode/edge cases, +2 persistent-mode); tool picker + B/M shortcuts; persistent combine mode buttons (New/+/−/∩) as WM-hijack-proof alternative to Alt modifier |
| S3 — Bucket fill + scanline flood | ✅ done | 2026-04-20; 97 tests passing (+11 in `test_fill`); G shortcut + options row (Tolerance 0–255, Opacity 0–100%); respects selection and paint target; PaintCommand undo |
| S4 — Magic wand (smart selection) | ✅ done | 2026-04-20; 104 tests passing (+7 wand/floodSelect); W shortcut; options row (Tolerance + combine-mode buttons); Shift/Alt modifiers + persistent combine mode; uses shared `floodSelect` |
| S5 — Crop tool + canvas resize | ✅ done | 2026-04-20; 117 tests passing (+13 in `test_crop`); C shortcut; drag-and-release commit; `CropCommand` deep-snapshot undo (per-layer tile clone + mask + selection); cropped selection collapses to null when it doesn't intersect |
| S6 — `.txl` native file format v1 | pending | — |
| S7 — Verify + tag `v0.1.0-m1` | pending | — |

## Previous Milestone: M0 — Minimal Editor Bootstrap ✅

**Definition of Done:** Runnable Qt6 app that opens PNG, stacks pixel layers with opacity + 13 PS blend modes, paints with a round brush on layer or raster mask, undo/redo, exports flat PNG. **Shipped 2026-04-20 as tag `v0.0.1-m0`.**

## Step Progress

| Step | State | Notes |
|------|-------|-------|
| S1 — Bootstrap (apt install, git init, workflow docs) | ✅ done | 2026-04-20 |
| S2 — CMake + hello Qt window | ✅ done | 2026-04-20; `./build/tuxels` launches |
| S3 — Core: Tile, TileStore, TuxImage | ✅ done | 2026-04-20; 15 unit tests pass |
| S4 — Layers + compositor + 13 blend modes | ✅ done | 2026-04-20; 43 unit tests pass |
| S5 — UI shell: CanvasView + LayersPanel | ✅ done | 2026-04-20; window hosts sample 3-layer doc |
| S6 — PNG I/O + File menu | ✅ done | 2026-04-20; PngIO round-trip unit tests pass |
| S7 — Brush engine + BrushTool | ✅ done | 2026-04-20; 54 tests passing |
| S8 — Undo/redo with tile COW | ✅ done | 2026-04-20; 59 tests passing |
| S9 — Layer masks in UI | ✅ done | 2026-04-20; 59 tests passing |
| S10 — Verify + tag v0.0.1-m0 | ✅ done | 2026-04-20; ToolsPanel + partial-recompose + Command::dirtyRect undo path + brush cursor ring; user re-verified all 13 DoD items; tagged `v0.0.1-m0`; 62 tests |

## What Works Right Now

- `cmake -S . -B build -G Ninja && cmake --build build` produces `./build/tuxels`.
- Launching the binary opens a Qt6 main window with Photoshop-style menu stubs (File/Edit/Image/Layer/Select/Filter/View/Window/Help) and a status-bar "Ready".
- Verified headless: `timeout 2 ./build/tuxels -platform offscreen` runs for 2s without crash.
- Canvas shows a composited 1024×768 sample document (white bg, red rectangle, green disc blended Multiply); checkerboard around canvas; Ctrl+wheel zoom, middle/shift+drag pan.
- Layers dock (right) lists layers top-down with per-row visibility toggle, thumbnail, name, blend-mode combo, and opacity slider. Add/Delete/Up/Down via toolbar or Layer menu. Blend-mode or opacity changes trigger recomposite immediately.
- `File → Open…` imports an 8-bit PNG as a single pixel layer; `File → Export As PNG…` composites the document and writes an 8-bit sRGB PNG. PngIO round-trip verified in `test_png_io`.
- Click-drag on the canvas paints a round brush onto the active pixel layer. Brush kernel has hard/soft falloff (hardness ∈ [0,1]) and honors opacity × flow per stamp; spacing is diameter × 10%. `[` / `]` shrink/grow the brush diameter; status bar reports new size. Layer thumbnails refresh on stroke end.
- `Edit → Undo` / `Edit → Redo` (Ctrl+Z / Ctrl+Shift+Z) reverse and replay: paint strokes (via tile-COW snapshots — only touched tiles are recorded), add/delete/reorder layers, visibility toggles, blend-mode changes, and opacity slider commits (one entry per drag, coalesced on release). Undo depth: 64.
- `Layer → Add Layer Mask` attaches a white (fully-reveal) raster mask to the active pixel layer and switches paint target to mask. The layers panel shows a second thumbnail next to the layer thumb; click a thumb to choose paint target (layer or mask), shift-click the mask thumb to toggle enabled (disabled mask has a red tint overlay), right-click for "Delete Mask". All mask ops are undoable.
- **Tools dock (left)**: foreground/background color swatches (click → color picker, X swaps, D resets to black/white) plus size / hardness / opacity / flow sliders that drive the live brush. `[`/`]` and the dock stay in sync.
- **Paint latency fix**: `compose(tree, out, Rect)` overload restricts the tile loop to tiles touched by the current stamp; the canvas partial-recomposites and partial-uploads only those rows per mouse tick. Structural ops (layer add/delete/reorder, blend/opacity/visibility changes) still full-recompose; paint-stroke undo/redo goes through the partial path via `Command::dirtyRect()` which `PaintCommand` computes from its tile snapshot.
- **Brush cursor ring**: concentric black/white 1-px ellipses centered on the cursor, sized to the active tool's `cursorRadiusPx()` × zoom. Only the ring's widget rect is invalidated per mouse move (partial `update()`), so the preview is cheap. `[`/`]` refresh it live.
- **Selection model** (M1-S1): `core/SelectionMask` (1-channel over a document-sized `TuxImage`, tile-sparse fillRect/combine/invert/clone). `Document::selection()` is a nullable `unique_ptr<SelectionMask>`; null = "no selection". `BrushEngine` multiplies per-pixel deposit by `selection->sample(x,y)` so strokes are clipped to the active selection. `Select` menu wired: `All` (Ctrl+A), `Deselect` (Ctrl+D), `Inverse` (Ctrl+Shift+I) — each undoable via a dedicated `SelectionCommand`.
- **Marquee + ants** (M1-S2): ToolsPanel has a picker row with Brush (B) / Marquee (M) buttons. With the Marquee tool active, drag on the canvas builds a rectangular selection; modifiers at press time pick the combine mode — plain = Replace, Shift = Add, Alt = Subtract, Shift+Alt = Intersect. Live rubber-band dashed rect tracks the drag; on release a `SelectionCommand` commits the before/after pair (so the marquee is undoable). A 10 Hz `QTimer` animates the marching-ants overlay (black solid under white dashed, `dashOffset` rotates) around the selection boundary; only the selection's widget-space bbox repaints per tick. The Shift-for-pan gesture is suppressed when the Marquee is active, so Shift+Left adds to the selection instead of panning.
- **Marquee persistent mode** (M1-S2 follow-up): ToolsPanel reveals a Marquee options row with four combine-mode buttons (New / + / − / ∩) when the Marquee tool is active. The selected button drives the combine mode for drags with no modifier keys held at press time; holding Shift / Alt / Shift+Alt still temporarily overrides per Photoshop. This keeps Subtract and Intersect reachable on Linux WMs (GNOME default) that consume Alt-drag for window-move before Qt receives the event.
- **Paint bucket** (M1-S3): ToolsPanel picker row gains a Bucket (G) button; G shortcut switches tools. When the Bucket tool is active, a press on the canvas runs a 4-connected scanline flood fill at that pixel on the active paint target (layer or mask), comparing against the seed's source color with an L∞ tolerance threshold (UI slider 0–255, normalized to [0,1]). Fill is soft-masked by the selection (binary threshold for traversal; per-pixel multiply on the fill alpha for feathered edges) so bucket-inside-selection just works. A Bucket options row (Tolerance / Opacity) appears when the tool is active. The foreground swatch is the shared source of truth for both brush color and fill color. Fill tile-COW snapshots are wrapped in a `PaintCommand`, so each click is one undo step. Non-contiguous (same-color-anywhere) fills are deferred.
- **Magic wand** (M1-S4): picker row gains a Wand (W) button; W shortcut switches tools. A click on the canvas samples the active PixelLayer's pixel color, runs a scanline flood (`floodSelect` in `fill/FloodFill`) with L∞ tolerance, and commits a `SelectionCommand` carrying the combined before/after selection. Combine-mode semantics match the marquee (Replace/Add/Subtract/Intersect — modifiers win when held, persistent-mode buttons cover WM Alt-hijack). Subtract/Intersect further clip the wand traversal to the existing selection so the wand can never escape it. Shift-click on the canvas routes to the wand (not the pan gesture) because `consumesShiftClick()` returns true.
- **Crop** (M1-S5): picker row gains a Crop (C) button; C shortcut switches tools. A drag on the canvas defines the crop rectangle (rubber-band rendered via the new generic `ToolBase::liveRect()` virtual — shared with the marquee). On release with a non-degenerate rect the tool produces a `PendingCrop{rect}` that MainWindow converts into a `CropCommand`. The command's constructor deep-snapshots the pre-crop document (tile-clones of every PixelLayer image + any mask + the active selection), then calls `applyCropInPlace` which replaces each layer's image with a cropped copy, crops the selection mask to the new doc space (collapsing to null if nothing remains selected), and resizes the document. Undo re-installs the deep snapshot; redo re-runs the crop math. Degenerate drags (< 2 px in either axis) are ignored. Crop respects the document bounds by clamping in `CropTool::release`. Interactive Photoshop-style "drag → adjust handles → Enter to commit" is deferred; current behavior is drag-to-commit with Ctrl+Z to recover.

## What Is Broken / Known Issues

- None (no code yet).

## Open Questions

- See `ARCHITECTURE.md` "Open Questions" section. None currently blocking.

## For Cold-Start Claude

1. Read this file.
2. Read `NEXT.md` — exact next actions.
3. Read `ARCHITECTURE.md` — as-built decisions, don't re-derive.
4. Read `BUILD.md` if build-related.
5. Only after that, `SCOPE.md` for the big picture (long — skim table of contents).
6. The approved plan is at `/home/james/.claude/plans/steady-framing-willow.md`.
