# Tuxels — As-Built Architecture

Supplements (does not duplicate) `/home/james/Tuxels/SCOPE.md`. SCOPE.md is the aspirational blueprint; this file captures concrete decisions as we implement. If a decision here conflicts with SCOPE.md, this file wins (and note the deviation).

---

## 1. Pixel Format (Internal)

```cpp
struct Rgba32F { float r, g, b, a; };  // 16 bytes, aligned
```

- 32-bit float per channel; always, regardless of source/target bit depth.
- **Alpha semantics (internal):** straight alpha (not premultiplied). Blend-mode math is per-channel on straight values. Conversion to premultiplied only at compositing "over" boundaries and GPU upload (later phase).
- Why: SCOPE.md §5.4.2 — "internal compositing pipeline should always operate in at least 32-bit float." Also avoids 8-bit banding under heavy edits.
- Future: CMYK/Lab/Grayscale modes will be separate pixel types or a tagged-channel generalization. M0 is RGBA only.

## 2. Tile Size and Layout

- Fixed **256 × 256 pixels** per tile for M0. Constant lives in `core/Tile.h` as `kTilePx = 256`.
- Each tile is a contiguous `std::array<Rgba32F, kTilePx * kTilePx>` on the heap (via `std::unique_ptr`). Row-major, origin top-left.
- Tiles are the unit of: sparse allocation, dirty invalidation, undo snapshot, and (later) GPU upload.
- A `TileStore` holds `std::unordered_map<TileCoord, std::shared_ptr<Tile>>`. Absent tiles are conceptually fully-transparent.
- Copy-on-write: when a command needs to snapshot, it stashes the `shared_ptr<Tile>` for later restore and allocates a fresh writable tile.

## 3. Image Type

`TuxImage`:
- `int width, height` in pixels.
- `TileStore tiles`.
- Methods: `Rgba32F getPixel(int,int) const`, `void setPixel(int,int,Rgba32F)`, `Tile* getOrCreateTile(TileCoord)`, `const Tile* findTile(TileCoord) const`, `void forEachTile(callback)`, `Rect boundingBoxOfAllocated() const`.
- No width/height padding to tile boundary — the canvas logical size is exact; tiles at the right/bottom edges are cropped when read.

## 4. Layer Tree

- M0: flat ordered list `std::vector<std::unique_ptr<LayerBase>>`. Index 0 = bottom layer.
- `LayerBase` abstract API:
  - `LayerID id` (monotonic uint64)
  - `std::string name`
  - `bool visible`
  - `float opacity ∈ [0, 1]`
  - `BlendMode blend` (enum)
  - `std::unique_ptr<LayerMask> mask` (nullable)
  - virtual `void renderTile(TileCoord tc, Rgba32F* out) const` — produces the layer's contribution tile for compositing (before mask/opacity/blend).
- Concrete M0 subclasses:
  - `PixelLayer` — owns a `TuxImage`.
- Future: `AdjustmentLayer`, `GroupLayer`, `SmartObjectLayer`, `TextLayer` (not in M0).

## 5. Masks

- `LayerMask` = `TuxImage` but semantically grayscale (only `.r` read; `.g`=`.b`=`.r` when painted). In M0, reuse `Rgba32F` for simplicity; optimize to an 8-/16-bit single-channel later.
- Mask is applied by multiplying layer-contribution alpha by mask intensity before blend.
- `enabled` flag (shift-click toggles).
- Default mask fill = 1.0 (fully reveals).

## 6. Blend Modes (M0 set: 13)

Implemented in `src/compositor/blend.cpp`. Formulas operate per-channel on *straight-alpha* float values in [0, 1]; result alpha = `sa + da*(1-sa)`; result color = Porter-Duff "over" with the mode's compositing function substituted for the standard `src*sa + dst*(1-sa)`.

```
Normal:       C = Cs
Dissolve:     C = Cs if rand() < sa else Cd   (stochastic; seeded per-stroke for repeatability)
Darken:       C = min(Cs, Cd)
Multiply:     C = Cs * Cd
Color Burn:   C = 1 - (1 - Cd) / Cs   (Cs==0 → 0;  careful with divide-by-zero)
Lighten:      C = max(Cs, Cd)
Screen:       C = 1 - (1 - Cs) * (1 - Cd)
Color Dodge:  C = Cd / (1 - Cs)   (Cs==1 → 1)
Overlay:      C = if Cd <= 0.5: 2*Cs*Cd  else: 1 - 2*(1-Cs)*(1-Cd)
Soft Light:   (Photoshop variant — not W3C)
              if Cs <= 0.5: C = Cd - (1 - 2*Cs) * Cd * (1 - Cd)
              else:          C = Cd + (2*Cs - 1) * (D(Cd) - Cd)
              where D(Cd) = ((16*Cd - 12) * Cd + 4) * Cd   if Cd <= 0.25 else sqrt(Cd)
Hard Light:   symmetric of Overlay: switch on Cs.
Difference:   C = |Cs - Cd|
Exclusion:    C = Cs + Cd - 2*Cs*Cd
```

- Unit tests per mode: at least 3 input/output pairs per mode, values checked to ±1e-5.
- Deferred (remaining 14 PS modes): Linear Burn, Linear Dodge (Add), Vivid Light, Linear Light, Pin Light, Hard Mix, Subtract, Divide, Hue, Saturation, Color, Luminosity, Darker Color, Lighter Color. Scheduled for Phase 2 of SCOPE.md.

## 7. Compositor

- Two overloads in `src/compositor/compose.cpp`:
  - `compose(tree, out)` — full recompose over all tiles intersecting the image.
  - `compose(tree, out, Rect pixelRect)` — restrict the tile loop to tiles intersecting `pixelRect` (clipped to image bounds). Used on the paint hot path so a brush stamp only re-touches the tiles it lands on. Both paths go through the same inner `composeTileRange` helper.
- Walk tile coordinates spanning the target range. For each tile coord:
  - Start with transparent accumulator (`Rgba32F{0,0,0,0}`).
  - For each layer (bottom to top) if visible:
    - `renderTile` → get layer tile contribution.
    - Multiply layer alpha by `opacity` and mask value (if mask present and enabled).
    - Blend using `layer.blend` against accumulator.
  - Write accumulator into the corresponding tile of `out`.
- **Dirty-rect path for paint:** `BrushEngine` tracks `incremental_` in addition to total `bounds_`; each stamp grows both. `ToolBase::takeDirtyRect()` (default empty, `BrushTool` forwards from the engine) drains the rect. `CanvasView` unions incoming rects into `dirtyRect_`; its `paintEvent` calls `compose(tree, out, dirtyRect_)` when set (partial) or `compose(tree, out)` otherwise (full). Only the rows of the cached `QImage` inside the dirty rect are re-quantized, and `QWidget::update(QRect)` limits Qt's repaint area. Full recompose is still used for structural ops (layer reorder, blend/opacity/visibility change, mask toggle, undo/redo) where the dirty region isn't trivially available.
- Single-threaded CPU for M0. Multi-thread per-tile (std::async or thread pool) is a simple drop-in for later.

## 8. Color Management (M0 scope)

- sRGB passthrough only. PNG load: `QImage::Format_RGBA8888` → divide by 255 → Rgba32F.
- Export: Rgba32F → multiply by 255 → clamp → `QImage::Format_RGBA8888` → write as PNG via Qt.
- No gamma linearization yet (known limitation; will be addressed alongside lcms2 transforms when working-space selection is added).
- `ColorSpace` placeholder type exists so that per-image color spaces can be attached later without callers changing.

## 9. Undo/Redo

- `Command` abstract: `void execute()`, `void undo()`, `size_t memoryEstimate() const`.
- `UndoStack`: `std::vector<std::unique_ptr<Command>>` + cursor index.
- `PaintCommand`:
  - Snapshots `std::map<TileCoord, std::shared_ptr<Tile>>` of pre-stroke tiles (COW — cheap because we swap in fresh writable tiles before painting).
  - On undo: swap pre-stroke tiles back into place.
  - Begins on mouse-down, accumulates during drag, commits on mouse-up.
- `LayerOpCommand`: add/delete/reorder/rename/blend-mode-change — snapshots relevant metadata.
- No merging of consecutive commands in M0.
- Memory budget unbounded in M0; add a cap in Phase 2.

## 10. Canvas Widget

- `CanvasView : public QOpenGLWidget`.
- Keeps a `QOpenGLTexture` with the composited `TuxImage` uploaded as RGBA8 for display.
- paintGL: draw a fullscreen quad with the texture, respecting zoom + pan transform.
- Mouse events forwarded to the active `Tool`.
- Zoom: Ctrl+scroll, centered on cursor. Pan: middle-drag or space-drag.

## 11. Tools

- `ToolBase` abstract: `press` / `move` / `release` with image-space (x, y) + `MouseButton`. Plus `takeDirtyRect()` returning the pixels newly dirtied since the last call (default empty; `BrushTool` overrides to drain the engine's incremental rect).
- `BrushTool` for M0. Move/Select/etc. are stubs.
- Active tool held by `MainWindow`, forwarded to `CanvasView` via `setTool()`.

### ToolsPanel (UI — left dock)

- `src/ui/ToolsPanel.{h,cpp}`. QDockWidget parked in `Qt::LeftDockWidgetArea`.
- Foreground/background color swatches: `QColorDialog` on click; `X` (also a global shortcut) swaps; `D` resets to black/white.
- Sliders for size (1–500 + a 1–2048 spinbox for precise entry), hardness / opacity / flow (0–100%, mapped to the brush's 0–1 params).
- No intermediate model: edits go straight into the live `RoundBrush`. `refreshFromBrush()` pulls current params back out so `[`/`]` diameter nudges stay in sync.
- Internal `Swatch` widget is a plain `QFrame` subclass in an anonymous namespace, no `Q_OBJECT` — uses a stored `std::function` callback instead of a signal, which keeps it off AutoMOC's radar and fully self-contained in the `.cpp`.

## 12. Brush

- `RoundBrush`:
  - `float diameter` (pixels), `float hardness ∈ [0,1]`, `float opacity ∈ [0,1]`, `float flow ∈ [0,1]`, `float spacingRatio` (default 0.1, i.e., stamps every 10% of diameter along the stroke).
  - Precompute `stampKernel` (2D array of coverage 0..1) when diameter/hardness changes. Falloff: smoothstep from hardness*radius to radius.
- `BrushTool::stroke()` accumulates distance and drops stamps at `spacingRatio * diameter` intervals. Each stamp blends its kernel into the target buffer using a per-pixel "over" with color × stamp × flow. Opacity caps accumulated stamp coverage per-stroke (Photoshop "opacity" semantics, approximately).
- Paint target: active layer's `TuxImage` OR its mask's `TuxImage` if mask is selected.

## 13. File I/O (M0)

- Only PNG via Qt's built-in loader (wraps libpng). `tests/fixtures/test_rgba.png` generated programmatically inside a test — no binary checked in initially.

## 14. Selection (M1)

- `core/SelectionMask.{h,cpp}` wraps a document-sized `TuxImage` with intensity in the R channel (0 = not selected, 1 = fully selected, partial values reserved for feathered/AA selections in future milestones).
- `Document::selection()` is `std::unique_ptr<SelectionMask>`; a null pointer is the canonical "no selection — paint everywhere" state. We deliberately do **not** produce an all-zero SelectionMask — Deselect drops the pointer instead, so the brush engine's "no selection" fast path (one null check) covers the common case.
- **Selections are edit-time, not display-time.** The compositor ignores the selection; tools consult it when writing pixels. The brush engine takes a `const SelectionMask*` and multiplies per-pixel deposit by `sample(x, y)`. Future bucket / wand tools read the same mask.
- Operations exposed: `fillRect(Rect, value)`, `combine(other, SelectionMode)`, `invert()`, `clone()`, `makeAll(w, h)`. `SelectionMode = Replace | Add | Subtract | Intersect` maps to per-pixel replace / max / `a*(1-b)` / min and will be driven by modifier keys on the marquee tool (S2).
- Tile-sparse: `fillRect` over an empty region of a 4000×4000 doc only allocates the touched tiles. `combine`/`invert`/`clone` walk tiles directly (no per-pixel hash) so they scale with the populated area, not the whole canvas.
- `SelectionCommand` (in `history/`) pushes a (before, after) pair of unique_ptr<SelectionMask>; undo/redo clone them into `Document::setSelection` so command-held copies stay independent of further edits.
- Select menu wired in MainWindow: `Select → All` (Ctrl+A), `Deselect` (Ctrl+D), `Inverse` (Ctrl+Shift+I). Inverse is no-op when nothing is selected (matches PS's greyed-out state).

### Rectangular marquee + marching ants (M1-S2)

- `tools/MarqueeTool.{h,cpp}` is a `ToolBase` subclass. On press it records the drag origin and the current modifier bitmask; on release it builds a fresh doc-sized `SelectionMask` with the drag rect filled, combines with the existing selection (if any) per the mode derived from modifiers, and stashes a `PendingCommit{before, after, label}` for MainWindow to convert into a `SelectionCommand`.
- Modifier mapping: **none → Replace, Shift → Add, Alt → Subtract, Shift+Alt → Intersect**. Read once at press time; further modifier changes mid-drag do not retroactively switch modes (consistent with Photoshop).
- Collapse discipline: if `combine(...)` leaves the mask all-zero (e.g. Subtract-all, Intersect with disjoint rect), `MarqueeTool::release` converts `after` back to `nullptr` so the brush's "null = no selection" fast path remains correct. An empty drag in Replace mode commits a Deselect when a prior selection existed, and is a no-op otherwise.
- `ToolBase` grew two hooks to support modifier-sensitive tools without a Qt dependency in core: `void setModifiers(int)` (called by CanvasView before each press/move/release) and `bool consumesShiftClick() const` (defaults false; `MarqueeTool` returns true). CanvasView's Shift+Left pan gesture is suppressed whenever the active tool claims shift, so the marquee's Shift-Add modifier takes precedence. `tools/ToolBase.h` exposes a Qt-free `Mod::{Shift, Alt, Ctrl}` bitmask; CanvasView translates `Qt::KeyboardModifiers` into it at the call site.
- Marching-ants overlay lives in `CanvasView`: `rebuildSelectionSegments()` walks the doc-pixel bounding rect of the selection and emits 4-connected boundary segments (one line per selected pixel whose 4-neighbor is unselected) into `selectionSegments_` (in doc coords). The cache rebuilds only when `doc_->selection()` pointer identity changes (menu ops, marquee commits, undo/redo all swap the `unique_ptr`, so identity is a reliable change signal). A `QTimer` at 10 Hz advances `antsPhase_` and calls `update(selectionWidgetRect())` — only the selection's widget-space bbox redraws per tick, not the full canvas. Rendering: a black solid underlay plus a white dashed overlay with `Qt::CustomDashLine` pattern `{4, 4}` and moving `dashOffset`. `QPen::setCosmetic(true)` keeps the 1-px line stroke invariant under the painter's `scale(zoom_, zoom_)`.
- Rubber-band: `CanvasView::paintEvent` `dynamic_cast<MarqueeTool*>` and, if `liveRect()` is present, paints a dashed rect at the drag extent. Mouse move/press/release explicitly `update()` the old + new rubber-band widget-rects so the band doesn't leave trails when dragged quickly.
- Tool picker: ToolsPanel grew a segmented button row above the brush parameters (Brush + Marquee, `QButtonGroup` exclusive). Non-brush tools hide the brush-params group (`brushGroup_->setVisible(false)`). B / M global `QAction` shortcuts on MainWindow call `setActiveTool(ToolId)` which swaps the `CanvasView::tool_` pointer and keeps the picker UI in sync via `ToolsPanel::setActiveTool(id)`.

---

## Deviations from SCOPE.md (intentional, M0-only)

| SCOPE.md item | M0 deviation | Rationale |
|---|---|---|
| Full color management (§5.4.3) | sRGB passthrough only | Correct ICC pipeline is a Phase 1 item; M0 is about the editing model |
| 27+ blend modes (§5.1.1) | 13 modes | Enough to prove correctness; remaining 14 are deterministic to add later |
| CMYK/Lab/Grayscale (§5.4.1) | RGBA only | Channel abstraction deferred to Phase 2 |
| GPU compositing (§5.2) | CPU, single-threaded | Phase 5 work per SCOPE.md's own schedule |
| PSD read/write (§5.1) | Not implemented | Phase 2; M0 is PNG-only |
| Adjustment layers, styles, smart objects, text | Deferred | Phase 2/4/5 per SCOPE.md |

## Open Questions

*(None blocking M0. Future decisions logged here as they arise.)*
