# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**S6 — PNG I/O + File menu (New / Open / Export As PNG)**

Currently `File → Open…` and `File → Export As PNG…` pop a placeholder QMessageBox
("wired in the next step (S6)"). Replace those with real implementations.

Files to create:
- `/home/james/Tuxels/src/io/PngIO.h` — declarations:
  - `std::optional<TuxImage> loadPng(const QString& path, QString* err = nullptr);`
  - `bool savePng(const QString& path, const TuxImage& img, QString* err = nullptr);`
  The API takes `QString` so callers can pass QFileDialog results directly; internals
  use `QImage` via `QImageReader`/`QImageWriter`. Keeps `tuxels_core` Qt-free by
  placing `PngIO` in the `tuxels` executable target (not `tuxels_core`).
- `/home/james/Tuxels/src/io/PngIO.cpp` — conversions:
  - Read: `QImage::Format_RGBA8888` → `Rgba32F` with `r = qRed()/255.f`, ..., `a = qAlpha()/255.f`. Treat input as sRGB but **do NOT linearize** for M0 (sRGB passthrough — noted in ARCHITECTURE.md §7).
  - Write: `Rgba32F` → `QImage::Format_RGBA8888` with `std::clamp(v, 0.f, 1.f) * 255.f + 0.5f`.
- Update `CMakeLists.txt`: add `src/io/PngIO.cpp` to the `tuxels` executable source list
  (NOT to `tuxels_core` — keeps the core library Qt-free).

Wire in `MainWindow.cpp`:
- `onFileOpen()`: `QFileDialog::getOpenFileName(this, tr("Open Image"), QString(), tr("PNG Images (*.png)"))`. On success, `loadPng` → replace current document with a new `Document` whose single PixelLayer is the loaded image. Refresh panel + canvas.
- `onFileExport()`: `QFileDialog::getSaveFileName(this, tr("Export PNG"), QString(), tr("PNG (*.png)"))`. Call `compose(doc_->tree(), out)` where `out` is a fresh `TuxImage(width, height)`, then `savePng`.
- `onFileNew()` already creates a blank document; no PNG I/O change needed, but ensure the new doc has at least one starter layer (already does — "Background").

Tests (optional for M0):
- `tests/test_png_io.cpp` — requires Qt for QImage, which would break the Qt-free core rule. **Skip** unless we extract a pure-C++ PNG helper. Manual verification via build + export is sufficient for M0.

Verification:
```
cmake --build build
./build/tuxels                 # interactive: File > Export As PNG
# or round-trip:
./build/tuxels -platform offscreen   # headless instantiation
```
Open the exported PNG in an image viewer; should match the sample document
(white bg, red rectangle, green disc with Multiply blend).

Commit: "io: PNG load/save + File menu wiring".

## Then — S7: Brush engine + BrushTool

See plan file for detail. Will add `src/brush/{RoundBrush,BrushEngine}.{h,cpp}`
and `src/tools/{ToolBase,BrushTool}.{h,cpp}`. CanvasView will need mouse-event
forwarding to the active tool. Also need a Tools dock (simplest: a tiny toolbar
with "Move" and "Brush" buttons) and `[` / `]` key bindings for brush size.

## Future Steps (brief)

- S8: `src/history/` — UndoStack + PaintCommand (tile-COW) + LayerOpCommand.
- S9: Layer-mask UI wiring (Add Layer Mask menu item, mask thumb in LayerRowWidget, click-to-target, shift-click disable).
- S10: Manual verify of all DoD items + tag `v0.0.1-m0`.

See full detail in `/home/james/.claude/plans/modular-singing-teacup.md`.

## Cold-Start Checklist

If you just booted and are resuming:
1. `cat docs/STATUS.md` — current state.
2. This file — what to do next.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
4. `git log --oneline -10` — recent commits.
5. `cmake --build build && ctest --test-dir build` — sanity-check current tree.
6. Pick up "Immediately Next" above.
