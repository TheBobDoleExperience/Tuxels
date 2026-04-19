# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

Currently executing **S1 — Bootstrap**. Remaining S1 sub-tasks:
- [ ] Verify `sudo apt install` completed successfully (build-essential, cmake, ninja-build, qt6-base-dev, qt6-base-dev-tools, libqt6opengl6-dev, libqt6openglwidgets6, qt6-tools-dev, liblcms2-dev, libpng-dev, libjpeg-turbo8-dev, libtiff-dev, libwebp-dev, zlib1g-dev).
- [ ] `git init` in `/home/james/Tuxels/`, first commit: "chore: project scaffolding (SCOPE + docs + gitignore)".
- [ ] Mark task #1 completed; start task #2.

## Then — S2: CMake skeleton + hello Qt window

Files to create:
- `/home/james/Tuxels/CMakeLists.txt` — top-level. C++20, Qt6 `Widgets` + `OpenGLWidgets` + `Gui`, strict warnings (`-Wall -Wextra -Wpedantic -Werror=return-type`), optional `-fsanitize=address,undefined` under `TUXELS_SANITIZE`.
- `/home/james/Tuxels/src/app/main.cpp` — `QApplication`, instantiate `MainWindow`, `app.exec()`.
- `/home/james/Tuxels/src/app/MainWindow.h` / `.cpp` — empty `QMainWindow` subclass with menu bar stub (File/Edit/Layer menus, no actions yet) and a placeholder central widget.

Build + verify:
```
cmake -S . -B build -G Ninja
cmake --build build
./build/tuxels
```
A window should appear. Close it, commit: "scaffolding: CMake + Qt hello window".

## Future Steps (brief)

- S3: `src/core/` — Rgba32F, Tile (256×256), TileStore, TuxImage.
- S4: `src/layers/` + `src/compositor/` — layer tree, 13 blend-mode formulas, compose().
- S5: `src/ui/` — CanvasView (QOpenGLWidget), LayersPanel.
- S6: `src/io/PngIO` — load/save via QImage bridge, File menu actions.
- S7: `src/brush/` + `src/tools/BrushTool` — round brush stamping.
- S8: `src/history/` — UndoStack + PaintCommand with tile COW.
- S9: Layer-mask UI wiring.
- S10: Manual verify + tag `v0.0.1-m0`.

See full detail in `/home/james/.claude/plans/modular-singing-teacup.md`.

## Cold-Start Checklist

If you just booted and are resuming:
1. `cat docs/STATUS.md` — current state.
2. This file — what to do next.
3. `git log --oneline -10` — recent commits.
4. `ls build/tuxels 2>/dev/null && ./build/tuxels --version` — does the binary build/run?
5. Pick up at the first unchecked item above.
