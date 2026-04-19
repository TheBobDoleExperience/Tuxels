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
- **S5 completed**: UI shell wired. `src/core/Document.h` (Qt-free model holding width/height + LayerTree + active index). `src/ui/CanvasView.{h,cpp}` (QWidget — simplified from QOpenGLWidget for M0; checkerboard backdrop, cached QImage composite, Ctrl+wheel zoom, middle/shift+drag pan). `src/ui/LayerRowWidget.{h,cpp}` (per-row visibility/thumb/name/blend-combo/opacity). `src/ui/LayersPanel.{h,cpp}` (QDockWidget with QToolBar + QListWidget; renders top-down). `src/app/MainWindow.{h,cpp}` rewritten: owns Document, hosts CanvasView centrally and LayersPanel on right, Layer menu with add/delete, populates a 3-layer sample doc (white bg / red rect / green Multiply disc). Build error in `MainWindow.cpp:54` (`qApp` used without `<QApplication>` include) — fixed by adding the include. Headless run OK via `-platform offscreen`.
