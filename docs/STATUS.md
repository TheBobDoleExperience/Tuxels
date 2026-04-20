# Tuxels — Current Status

**One-paragraph summary:** Milestone **M0 shipped** as `v0.0.1-m0` on 2026-04-20 (commit `be87b70`). **M1 in progress** — selection infrastructure landed in S1 (SelectionMask data structure + brush clipping + Select menu). 75 unit tests green. Plan: `/home/james/.claude/plans/steady-framing-willow.md`.

## Current Milestone: M1 — Selection, Fill, and Native Format

**Definition of Done:** rectangular marquee + marching ants, bucket fill, magic wand, crop, `.txl` native file format round-tripping doc + layers + masks + selection. See plan file for detailed DoD.

### M1 Step Progress

| Step | State | Notes |
|------|-------|-------|
| S1 — SelectionMask + brush clipping + Select menu | ✅ done | 2026-04-20; 75 tests passing (13 new in `test_selection`) |
| S2 — Rectangular marquee + marching ants | pending | — |
| S3 — Bucket fill + scanline flood | pending | — |
| S4 — Magic wand (smart selection) | pending | — |
| S5 — Crop tool + canvas resize | pending | — |
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
- **Selection model** (M1-S1): `core/SelectionMask` (1-channel over a document-sized `TuxImage`, tile-sparse fillRect/combine/invert/clone). `Document::selection()` is a nullable `unique_ptr<SelectionMask>`; null = "no selection". `BrushEngine` multiplies per-pixel deposit by `selection->sample(x,y)` so strokes are clipped to the active selection. `Select` menu wired: `All` (Ctrl+A), `Deselect` (Ctrl+D), `Inverse` (Ctrl+Shift+I) — each undoable via a dedicated `SelectionCommand`. No UI to **create** non-trivial selections yet; arrives with the marquee tool in S2.

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
