# Tuxels — Current Status

**One-paragraph summary:** Project bootstrap in progress. SCOPE.md (aspirational blueprint) was written 2026-04-17 and is the single source of truth for long-term vision. Milestone M0 (minimal editor: layers + brush + blend + mask + undo + PNG I/O) is planned at `/home/james/.claude/plans/modular-singing-teacup.md`. As of 2026-04-20 we are in step **S1** — scaffolding and workflow infrastructure.

## Current Milestone: M0 — Minimal Editor Bootstrap

**Definition of Done:** Runnable Qt6 app that opens PNG, stacks pixel layers with opacity + 13 PS blend modes, paints with a round brush on layer or raster mask, undo/redo, exports flat PNG.

## Step Progress

| Step | State | Notes |
|------|-------|-------|
| S1 — Bootstrap (apt install, git init, workflow docs) | in_progress | apt install running; docs being written |
| S2 — CMake + hello Qt window | pending | |
| S3 — Core: Tile, TileStore, TuxImage | pending | |
| S4 — Layers + compositor + 13 blend modes | pending | |
| S5 — UI shell: CanvasView + LayersPanel | pending | |
| S6 — PNG I/O + File menu | pending | |
| S7 — Brush engine + BrushTool | pending | |
| S8 — Undo/redo with tile COW | pending | |
| S9 — Layer masks in UI | pending | |
| S10 — Verify + tag v0.0.1-m0 | pending | |

## What Works Right Now

- Nothing executes yet. Tree has: `SCOPE.md`, `.gitignore`, `docs/`.

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
