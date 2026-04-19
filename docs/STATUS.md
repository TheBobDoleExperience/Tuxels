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
| S7 — Brush engine + BrushTool | in_progress | |
| S8 — Undo/redo with tile COW | pending | |
| S9 — Layer masks in UI | pending | |
| S10 — Verify + tag v0.0.1-m0 | pending | |

## What Works Right Now

- `cmake -S . -B build -G Ninja && cmake --build build` produces `./build/tuxels`.
- Launching the binary opens a Qt6 main window with Photoshop-style menu stubs (File/Edit/Image/Layer/Select/Filter/View/Window/Help) and a status-bar "Ready".
- Verified headless: `timeout 2 ./build/tuxels -platform offscreen` runs for 2s without crash.
- Canvas shows a composited 1024×768 sample document (white bg, red rectangle, green disc blended Multiply); checkerboard around canvas; Ctrl+wheel zoom, middle/shift+drag pan.
- Layers dock (right) lists layers top-down with per-row visibility toggle, thumbnail, name, blend-mode combo, and opacity slider. Add/Delete/Up/Down via toolbar or Layer menu. Blend-mode or opacity changes trigger recomposite immediately.
- `File → Open…` imports an 8-bit PNG as a single pixel layer; `File → Export As PNG…` composites the document and writes an 8-bit sRGB PNG. PngIO round-trip verified in `test_png_io`.

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
