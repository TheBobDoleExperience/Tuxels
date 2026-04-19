# Tuxels — Current Status

**One-paragraph summary:** Project bootstrap in progress. SCOPE.md (aspirational blueprint) was written 2026-04-17 and is the single source of truth for long-term vision. Milestone M0 (minimal editor: layers + brush + blend + mask + undo + PNG I/O) is planned at `/home/james/.claude/plans/modular-singing-teacup.md`. As of 2026-04-20 we are in step **S1** — scaffolding and workflow infrastructure.

## Current Milestone: M0 — Minimal Editor Bootstrap

**Definition of Done:** Runnable Qt6 app that opens PNG, stacks pixel layers with opacity + 13 PS blend modes, paints with a round brush on layer or raster mask, undo/redo, exports flat PNG.

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
| S10 — Verify + tag v0.0.1-m0 | in_progress | 2026-04-20; user is mid-walkthrough, tag deferred until sign-off |

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
6. The approved plan is at `/home/james/.claude/plans/modular-singing-teacup.md`.
