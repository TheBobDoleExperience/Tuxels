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
- M5-S0: tree is now **recursive** via `LayerTree`. Root holds an ordered list of layers (bottom → top, index 0 = bottom); `GroupLayer` nodes own their own ordered child list recursively. The flat-list API (`size()`, `at()`, `move(from,to)`, `insertAt`, `removeAt`) is preserved and operates on root-level children. New path-based helpers (`findById`, `locate(id) → {parent, index}`, `flatten()`, `forEach(fn)`, `insertAtPath`, `removeFromPath`, cross-parent `move(fromParent, fromIdx, toParent, toIdx)`) live in `src/layers/LayerTree.cpp`. `flatten()` returns layers in depth-first **child-then-self** order (bottom-up compose order; the IO writer also uses this).
- `LayerBase` abstract API:
  - `LayerID id` (monotonic uint64)
  - `std::string name`
  - `bool visible`
  - `float opacity ∈ [0, 1]`
  - `BlendMode blend` (enum; `PassThrough` is the M5 default for groups)
  - `std::unique_ptr<LayerMask> mask` (nullable)
  - `int originX, originY` — doc-coord of the layer's (0, 0) pixel (M2-S0).
  - `bool clipToBelow` (M4-S1) — adjustment / group effect gated by the per-pixel alpha of the most recent non-clipped pixel layer below.
  - virtual `bool renderTile(TileCoord tc, Rgba32F* out) const` — produces the layer's contribution tile in **doc-coord tile space**, translating by origin and folding the mask into output alpha so `compose()` itself is origin-unaware. Returns false when the tile doesn't intersect the layer's content rect.
- Concrete subclasses:
  - `PixelLayer` — owns a `TuxImage` sized to the layer's content (not necessarily doc-sized).
  - `AdjustmentLayer` (abstract, M3-S0) — no backing image; subclasses (Levels, Curves, Hue/Sat, B&C) override `applyToAccum`. `renderTile` returns false.
  - `GroupLayer` (M5-S0) — owns `std::vector<std::unique_ptr<LayerBase>> children` plus a UI-only `bool isExpanded` (persisted in `.txl` v5). Default blend is `BlendMode::PassThrough`. `renderTile` returns false (compose recursion handles groups directly).
- **Origin invariants** (M2-S0):
  - Masks share their owning layer's origin AND dimensions *for pixel layers*. Creating a mask sizes it to `layer->image.{width,height}()`, not doc dims.
  - `Document::addBlankPixelLayer(name)` still creates a doc-sized layer at origin `(0, 0)`; `Document::addPixelLayer(TuxImage&&, int ox, int oy, name)` installs a layer at an arbitrary origin (used by Place Image).
  - Crop intersects each layer's doc-space content rect with the crop rect; surviving rect becomes the new image, origin shifts into the new doc coord frame, non-overlapping layers drop to 0×0 (and lose their mask). M5: `applyCropInPlace` walks via `tree.forEach` so children inside groups crop correctly.
- **Layer kind discriminator** — `LayerBase::kind()` returns `LayerKind::Pixel` / `Adjustment` / `Group`. The compositor dispatches on this:
  - **Pixel** → `renderTile` + blend.
  - **Adjustment** → `applyToAccum` + lerp-back. Doc-sized mask; `Document::addAdjustmentLayer<T>` auto-attaches a doc-sized white mask with `enabled = true` and flips `paintTarget` to `Mask`. Origin is always `(0, 0)`. M5-S5: addAdjustment slots route the new layer into the active group / above the active layer (not always root) via `MainWindow::computeAdjustmentInsertSlot`.
  - **Group** → recursive compose via `composeChildren`; see §7.
- **Active layer** (M5-S0): tracked by `LayerId activeLayerId_` on `Document` (was `int activeLayerIndex_`). `Document::activeLayer()` resolves via `tree_.findById`. The legacy `activeLayerIndex()` getter/setter remains as a transitional shim that walks root-flat positions for code that hasn't migrated yet (mostly tests + a few root-only boundary checks in MainWindow).
- `LayerRowWidget` (M5-S3) renders three layer kinds:
  - **Pixel**: image thumbnail; left-click swaps `paintTarget` between layer + mask.
  - **Adjustment**: "fx" glyph thumbnail; left-click on the thumb emits `editAdjustmentRequested(LayerBase*)` (for the dialog-only kinds; non-modal kinds use the Properties dock).
  - **Group**: chevron `▾`/`▸` + folder-glyph thumb (`QStyle::SP_DirIcon`) over a tinted background. Chevron click toggles `isExpanded` and re-walks the panel; blend combo includes "Pass Through" (groups only). `setIndentDepth(int)` sets `4 + depth*12 px` left margin so children of expanded groups visually nest.
- Future: `SmartObjectLayer`, `TextLayer` (Phase 2+).

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
- M5-S1: per-tile body is factored into a recursive `composeChildren(ctx, layers, accum, lastBaseAlpha, hasBase)`. Root call walks `tree.raw()`; group dispatch recurses into `group->children` with shared or isolated state.
- For each layer in a child list (bottom to top), if visible + `opacity > 0`:
  - **Group + Pass-Through** (the `GroupLayer` default): hot-path inlines the recursion bit-exact with a flat list when `opacity == 1`, no mask, no clip. Otherwise snapshots `accum` into a scratch buffer, recurses into `group->children` with the parent's `accum` + `lastBaseAlpha` + `hasBase` (children share parent scope, so adjustments inside leak out and clipped adjustments inside gate against outer bases), then per-pixel lerps `accum = mix(accumBefore, accum, opacity * maskV)`. A clipped Pass-Through group with no base below = no-op (skip recursion entirely).
  - **Group + non-Pass-Through (isolated)**: allocates `accum2` (transparent) + scope-local `lastBaseAlpha2` + `hasBase2 = false`, recurses with these, then back-composites `accum2` into the parent `accum` via `compositePixel(parentAccum, src, group->blend, f, ...)` with `f = opacity * maskV * (clipToBelow ? lastBaseAlpha[idx] : 1)`. After back-composite, parent's `lastBaseAlpha` is updated from `accum2`'s alpha so a clipped adjustment immediately above the group gates against the group's contribution.
  - **Pixel path** (`kind() == LayerKind::Pixel`): `renderTile` → layer tile contribution (with origin translation + mask pre-multiplied in). Blend using `layer.blend` and `layer.opacity` against the accumulator. Capture this layer's source alpha into `lastBaseAlpha[]` (per-pixel) before blending, so subsequent clipped adjustments / groups gate against this layer's own contribution alpha rather than the post-blend accumulator alpha.
  - **Adjustment path** (M3-S0): copy the accumulator into a scratch buffer, call `applyToAccum(tc, scratch)` which the subclass rewrites in place, then lerp per pixel using `factor = opacity * maskV` (maskV = `1.0` if no mask/tile else `maskTile->at(px,py).r`; fetched once per compose tile because adjustment masks are doc-sized + tile-aligned). M4-S1: clipped adjustments multiply factor by `lastBaseAlpha[idx]`. Skip pixels where `accum.a <= 0`. Adjustment layers do NOT update `lastBaseAlpha` — they're transformations, not base content.
  - Write accumulator into the corresponding tile of `out`.
- Scratch buffers (`layerTile`, `adjScratch`) live in a `Ctx` struct + raw pointers — single allocation per outer `composeTileRange` call, shared across recursion levels (each layer fully consumes them within its iteration). Isolated groups allocate their own per-call `accum2` + `lastBaseAlpha2`.
- **Dirty-rect path for paint:** `BrushEngine` tracks `incremental_` in addition to total `bounds_`; each stamp grows both. `ToolBase::takeDirtyRect()` (default empty, `BrushTool` forwards from the engine) drains the rect. `CanvasView` unions incoming rects into `dirtyRect_`; its `paintEvent` calls `compose(tree, out, dirtyRect_)` when set (partial) or `compose(tree, out)` otherwise (full). Only the rows of the cached `QImage` inside the dirty rect are re-quantized, and `QWidget::update(QRect)` limits Qt's repaint area. Full recompose is still used for structural ops (layer reorder, blend/opacity/visibility change, mask toggle, undo/redo, group expand/collapse) where the dirty region isn't trivially available.
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

- `src/ui/ToolsPanel.{h,cpp}`. QDockWidget parked in `Qt::LeftDockWidgetArea`. Internal layout is a `QScrollArea` wrapping a vertical column: a fixed FG/BG color row pinned at top, then a vertical accordion of one `CollapsibleSection` per tool (M4-S0 architecture).
- Foreground/background color swatches: `QColorDialog` on click; `X` (also a global shortcut) swaps; `D` resets to black/white. Always visible — they're shared between Brush and Bucket and act as global state, not a Brush-section concern.
- Each tool has its own `CollapsibleSection` (`src/ui/CollapsibleSection.{h,cpp}`) holding the tool's options as the section body. Header click activates the tool (`toolPicked` signal); chevron click toggles the section's expansion. **Manual chevron is the only path that affects collapse state** — `setActiveTool` only flips highlights, never expands or collapses. The user's expand/collapse state is sticky across tool switches.
- Section order: Move (V), Marquee (M), Lasso (L), Polygonal Lasso (P), Magic Wand (W), Select By Color (⇧W), Crop (C), Brush (B), Paint Bucket (G), Free Transform (⌃T). Tools without options (Move, Crop, Free Transform) get a one-line tip QLabel as their body so the visual rhythm is uniform.
- Wand and SBC each have their own combine-mode + tolerance widgets; state is shared (changing tolerance in one section mirrors into the other; `setWandMode` reflects into both rows of buttons). Same pattern for Lasso ↔ PolyLasso.
- Sliders for brush params (size 1–500 + a 1–2048 spinbox for precise entry, hardness / opacity / flow 0–100%, size jitter / opacity jitter, spacing 1–100% → `spacingRatio ∈ [0.01, 1.0]`) live inside the Brush section's body and write straight into the live `RoundBrush`. `refreshFromBrush()` pulls current params back out so `[` / `]` diameter nudges from MainWindow stay in sync.
- Internal `Swatch` widget is a plain `QFrame` subclass in an anonymous namespace, no `Q_OBJECT` — uses a stored `std::function` callback instead of a signal, which keeps it off AutoMOC's radar and fully self-contained in the `.cpp`.
- Test-only accessor `ToolsPanel::sectionFor(ToolId)` returns the `CollapsibleSection*` for a given tool so unit tests can drive `simulateHeaderClick` and inspect `isActive` / `isExpanded` without screen scraping.

### CollapsibleSection (UI — accordion building block, M4-S0)

- `src/ui/CollapsibleSection.{h,cpp}`. Q_OBJECT widget. Header is a `QFrame` with three children: a 14-px-wide chevron `QLabel` (▾ expanded, ▸ collapsed), a bold name `QLabel`, and an optional shortcut hint `QLabel` aligned to the right. Body is a `QWidget` container that wraps the caller's installed body widget.
- Two distinct gestures dispatched via `eventFilter`. Header eventFilter catches mouse press anywhere in the header → emits `headerClicked` (panel maps to "activate this tool"). Chevron eventFilter catches mouse press on the chevron → toggles `isExpanded` and emits `chevronClicked`. Chevron's filter consumes its event so the header branch never sees a chevron click.
- `setActive(bool)` updates a stylesheet on the header — left-edge accent stripe + tinted background when active. Does NOT touch expansion. `setExpanded(bool)` shows/hides the body container, updates the chevron glyph.
- Test hooks `simulateHeaderClick` and `simulateChevronClick` drive the gestures programmatically without synthesizing Qt mouse events. `simulateChevronClick` performs the same toggle-then-emit as the eventFilter path.

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
- Persistent marquee combine mode (S2 follow-up): because GNOME/Metacity capture Alt-drag for window-move before Qt sees the press, the Alt-based Subtract/Intersect modes are unreachable via keyboard on default Linux desktops. `MarqueeTool::setMode(SelectionMode)` stores a persistent mode that's used when no Shift/Alt is held at press time; modifiers still win when actually received. ToolsPanel renders a four-button options row (New/+/−/∩) when the Marquee tool is active and emits `marqueeModeChanged(SelectionMode)` for MainWindow to wire into `MarqueeTool::setMode`. This pattern is the template for every future tool whose PS counterpart uses Alt as a modifier (crop, lasso, magic wand, eyedropper-while-brushing, …).

### Paint bucket + scanline flood (M1-S3)

- `fill/FloodFill.{h,cpp}` is a pure algorithm with no Qt dependency (lives in `tuxels_core`). `floodFill(target, seedX, seedY, fillColor, opts, selection)` runs a 4-connected scanline flood from the seed and returns a `FloodFillResult{bounds, changed, pixelsFilled}`.
- Seed-color capture is done once at the start (via `target.getPixel(seed)`); candidate pixels use `target.getPixel(x,y)` AND a `visited` bitmap — the `visited[y*W+x]` flag is the only thing that keeps a freshly-filled pixel from being re-tested. The bitmap is a dense `std::vector<uint8_t>` of W×H bytes. For a 4096×4096 canvas that's 16 MB — acceptable given fills are rare-vs-brush. A tile-sparse visited set is a deferred optimization.
- Tolerance metric: L∞ (max per-channel absolute delta) over R/G/B, normalized float. Alpha is ignored; for mask fills the extra channels are 0 so the max collapses to the R-delta automatically. UI slider is 0–255 / 255 to match Photoshop's exposed range.
- Selection integration: flood traversal only enters pixels where `selection == nullptr || selection->sample(x,y) > 0`. Within the selected region, the fill alpha is `opts.opacity * fillColor.a * selection->sample(x,y)` so feathered selections produce soft edges. Binary selections get a crisp clip for free.
- Blend math mirrors `BrushEngine::applyStamp` exactly — straight-alpha src-over with `a = opacity * fillColor.a * sel`, `out.rgb = a*fill + (1-a)*dst`, `out.a = a + (1-a)*dst.a`. Keeping bucket and brush on the same math avoids "my fill looks slightly different from my brush" bug reports.
- Scanline algorithm uses the classic Heckbert run-compression trick: at each cell we expand horizontally to find the full matching run `[xL, xR]`, fill+mark the run, then scan the two neighbor rows and push at most one seed per contiguous matching sub-run onto the stack. Reduces the work-list blow-up you'd get from a naive 4-pixel push-per-cell flood.
- `tools/BucketTool` is a `ToolBase` subclass that wraps FloodFill. Press dispatches the fill; move/release are no-ops. The tool wraps the fill in `target->beginRecord() / stopRecord()` so the same `PaintCommand` that records brush strokes also records fills — one click, one undo entry. `takeDirtyRect()` returns the fill bounds on press so CanvasView recomposites only the touched pixels.
- ToolsPanel: picker row grew a Bucket (G) button; a new bucket options group (Tolerance 0–255, Opacity 0–100%) is visible only when the Bucket tool is active. The foreground swatch is the single source of truth for both brush color and fill color — `applyFgToBrush()` pushes into both `brush_` and `bucket_`. MainWindow owns the `std::unique_ptr<BucketTool>` alongside the brush and marquee; `setActiveTool(ToolId::Bucket)` swaps the canvas tool pointer; `onLayerPainted()` drains `bucketTool_->takeLastFill()` and pushes a "Paint Bucket" PaintCommand.
- Known deferred work: non-contiguous fill (same-color-anywhere) is modeled as an opt field (`FloodFillOptions::contiguous`) but only the `true` branch is implemented. Fill blend modes beyond src-over are Phase 2 work.

### Magic wand (M1-S4)

- `fill/FloodFill` gained a sibling of `floodFill`: `floodSelect(source, seedX, seedY, tolerance, clip)` runs the same 4-connected scanline algorithm but writes the flooded region into a fresh `SelectionMask` instead of blending a color into a `TuxImage`. Same run-compression trick, same L∞-over-RGB tolerance metric, same visited bitmap. The `clip` parameter gates traversal to pixels inside an existing selection — used by Subtract/Intersect wand gestures so the wand can never leak past the current selection boundary.
- Runs collapse to `mask->fillRect({xL, y, xR-xL+1, 1}, 1.f)` so the result rides the SelectionMask's tile-sparse storage path: a large connected region in a 4k canvas only allocates the touched tiles. Returns `nullptr` when nothing matched (seed out of bounds / outside the clip / colors mismatch with tolerance=0) so callers can short-circuit.
- `tools/MagicWandTool` is a `ToolBase` subclass. Press samples the active PixelLayer's image at `(x, y)`, runs `floodSelect`, derives the combine mode from either the press-time modifiers (Shift/Alt/Shift+Alt) or the persistent mode (options row), and packages a `PendingCommit{before, after, label}` for MainWindow to convert into a `SelectionCommand` on the next `layerPainted` signal. Drag/release are no-ops; the gesture is a single click. `consumesShiftClick()` returns true so Shift+Left routes to the wand (add to selection) instead of the canvas's shift-pan gesture.
- Intersect/Subtract pass `doc.selection()` as the wand's traversal clip. Replace/Add pass `nullptr` so the wand can pick up pixels outside any pre-existing selection.
- Empty-result discipline: if `floodSelect` returns `nullptr` and the mode is Replace with an existing selection, the commit collapses to a Deselect. Any non-empty combined result that ends up all-zero (e.g. Subtract-all) is collapsed to `nullptr` via `SelectionMask::isEmpty()` so the brush's null-selection fast path stays correct.
- ToolsPanel: picker gains W; a new Magic Wand options group (same 4 combine-mode buttons as the Marquee group, plus a Tolerance slider) is visible only when Wand is active. `wandModeChanged(SelectionMode)` signal wires to `MagicWandTool::setMode`; this is the same hijack-proof pattern used for the marquee.

### Crop (M1-S5)

- `tools/CropTool` is a rubber-band `ToolBase` subclass: press sets the drag origin, move updates the rect, release (on a non-degenerate drag ≥ 2 px in either axis) clamps to doc bounds and stashes a `PendingCrop{rect}` that MainWindow drains in `onLayerPainted()` and converts into a `CropCommand`. Rect geometry is rendered via the new generic `ToolBase::liveRect()` virtual — the marquee's rubber-band path (previously keyed off a `dynamic_cast<MarqueeTool*>`) now hits the same code path, which also means future rect-drag tools get live rendering for free.
- `history/CropCommand` uses a single deep-snapshot undo strategy rather than storing both before and after. Construction captures `Snapshot{width, height, layers[{id, image, hasMask, maskImage, maskEnabled}], selection}` by tile-cloning every PixelLayer image, every attached mask, and (if present) cloning the selection. Then `applyCropInPlace(doc, rect)` replaces each layer's `image` with a cropped copy, crops masks the same way, runs `croppedSelection()` for the selection (collapsing to `nullptr` via `SelectionMask::isEmpty()` when the crop doesn't intersect any selected pixels), and finally calls `doc.setSize(rect.w, rect.h)`. `undo()` reinstalls the snapshot; `redo()` re-runs `applyCropInPlace` on the just-undone state (valid because undo/redo is strictly linear — no other commands can interleave between a paired undo and redo of the same command).
- Image copy is per-pixel via `src.getPixel → dst.setPixel`, skipping transparent (a ≤ 0) source pixels so the destination stays tile-sparse. Masks use the same path (absent tiles default to 1.0 = reveal, so skipping transparent sources yields correct "absent means unmasked" semantics without having to pre-fill the new mask with white). The `SelectionMask` crop walks the R channel directly and writes `Rgba32F(r, 0, 0, 1)` so alpha stays at 1 (present) for any non-zero selection intensity.
- `Document` gained a `setSize(w, h)` method; it's a raw setter that does not touch the layer tree (callers own the translate/resize of per-layer images to match). CropCommand is currently the only caller.
- UI wiring: ToolsPanel picker grew a Crop (C) button; MainWindow holds `std::unique_ptr<CropTool>`, routes the C shortcut, and on commit calls `canvas_->requestRecomposite()` + `refreshSelectionOverlay()` (dimensions changed, so partial recompose doesn't work — `CanvasView::recomposite` detects the mismatch and rebuilds `composite_`).
- Deferred: Photoshop-style interactive crop with persistent rect + adjustable handles + Enter-to-commit / Escape-to-cancel. Current behavior is drag-to-commit; Ctrl+Z recovers. Good enough for this milestone.

### `.txl` native format (M1-S6 → M2-S0)

- `io/TxlIO.{h,cpp}` — pure C++ (no Qt) so it lives in `tuxels_core` and tests link without pulling Qt. Chose a custom chunked binary container over ZIP: no external dependency, no platform packaging differences, and nothing about this v1 benefits from the deflate-per-file layout ZIP gives you. Trade-off accepted: the file is not inspectable with `unzip`.
- **Layout** (documented in `TxlIO.h`): 8-byte magic `"TUXELS\x01\x00"`, `Version u32` (`1` = legacy, `2` = current), `Flags u32 = 0`, `DocWidth/Height u32`, `ActiveLayer i32`, `PaintTarget u8`, `HasSelection u8`, `Reserved u16`, `NumLayers u32`. Each layer emits `{id u64, kind u8 (1=PixelLayer), visible u8, maskEnabled u8, hasMask u8, opacity f32, blend u32}` — and then (v2 only) `{LayerWidth u32, LayerHeight u32, OriginX i32, OriginY i32}` — followed by `{nameLen u32 + utf-8 name, numImageTiles u32 + [TileRecord], numMaskTiles u32 + [TileRecord]}`. A `TileRecord` is `{tx i32, ty i32, Rgba32F[kTilePixels]}` — one uncompressed 1 MiB payload per tile. Selection chunk at the tail mirrors one image.
- **v1 → v2 bump (M2-S0).** Added per-layer dims + origin because placed / transformed layers no longer match doc dims. v1 files remain readable: missing fields default to origin `(0, 0)` and layer dims == doc dims, which matches the legacy invariant (blank layer constructed against the doc size). Writers always emit v2 now — no "save v1" path.
- **No compression.** `Flags` is reserved for a future zstd bit; readers hard-fail on unknown flag bits so a v3 writer can't silently hand older readers something they can't decode.
- **Tile-sparse on disk.** We only emit tiles that exist in the `TileStore`, so a 4096×4096 canvas with a couple of strokes writes ~2 MiB, not 256 MiB. This matches how the in-memory `TuxImage` already works.
- **Endianness.** Host-order writes of primitive types via `reinterpret_cast` + `ofstream::write`. Tuxels only targets little-endian (Linux x86_64) for M1; porting to BE is a v2 task that can flip a `Flags` bit and byteswap. Floats go out in IEEE-754 host order for the same reason.
- **Mask semantics.** When `hasMask == false`, writers still emit `numMaskTiles u32 = 0` so the layer record has a uniform shape. Readers verify the zero to catch writer/reader drift early.
- **Layer-id counter.** `Document` gained a `setLayerIdCounter(LayerId)` setter; `loadTxl` seeds it with `max(loaded id)` so a subsequent `addBlankPixelLayer` call on a loaded document cannot collide with an id already in the tree.
- **Kind enum.** Only `1 = PixelLayer` is valid in v1. Future kinds (adjustment, text, group, smart object) get their own ordinals and readers branch on kind; older readers error cleanly on unknown kinds.
- **v3 (M3-S4).** Added kinds `2 = LevelsAdjustment`, `3 = CurvesAdjustment`, `4 = HueSaturation`, `5 = BrightnessContrast`. Adjustment kinds write `W=H=0`, `Origin=0`, `NumImageTiles=0` sentinel + a kind-specific descriptor (Levels: 4×5 floats; Curves: per-channel `u32 numPoints + (f32, f32) × N`; Hue/Sat: 3 floats; B&C: 2 floats), then masks through the existing flow. Adjustment masks reconstruct at doc dims (the auto-mask invariant from `Document::addAdjustmentLayer`). v1/v2 readers unchanged.
- **v4 (M4-S1).** Added `ClipToBelow u8` byte after `HasMask` in the layer record so PS-style clipping survives save/load. Pre-v4 records load with `clipToBelow == false`.
- **v5 (M5-S2).** Added kinds `10 = OpenGroup` (1-byte `IsExpanded` descriptor + doc-sized mask) and `11 = CloseGroup` (header-only marker, no payload after the standard header). Layer records remain a flat sequence on disk — nesting is reconstructed at load via a parse stack. **`NumLayers` semantic shifts** from "tree size" to "record count on disk" (a group contributes 2 records: open + close); documented in `TxlIO.h`. Group masks are doc-sized (extends the existing pixel-vs-non-pixel mask-dim branch — pixel masks are layer-sized, adjustment + group masks are doc-sized). v1–v4 still load with explicit version constants and gating flags (`hasOriginFields`, `acceptsAdjustmentKinds`, `hasClipByte`, `acceptsGroupKinds`). The reader maintains a `std::vector<GroupLayer*> groupStack` (empty = root); CloseGroup with an empty stack and EOF with a non-empty stack are both errors. The flat-with-section-dividers approach matches PSD §5.1.1 so a future PSD reader can reuse the same parse-stack pattern.
- **UI wiring.** `File → Open…` dispatches on extension (`.txl` → `loadTxl`, `.png` → `loadPng`); `File → Save As…` (Ctrl+Shift+S) writes `.txl` unconditionally (adds the extension if the user omits it). PNG export stays on its own menu entry — Save As is the native-format path, Export As PNG is the lossy-flatten path.

## 15. Properties Dock — Per-layer Editor (M4 + M6)

- `src/ui/PropertiesDock.{h,cpp}` — `QDockWidget` parked on the right, default tab-stacked under `LayersPanel`. Owns a `QStackedWidget` with one page per pane plus an empty-state page. Bind / unbind methods drive page switching: `bindLevels` (M4-S2), `bindCurves` (M4-S3), `bindHueSat` / `bindBrightnessContrast` (M4-S4), `bindGroup` (M6-S0), `bindNothing` (the catch-all).
- **Pane shape (uniform).** Every pane is a Q_OBJECT QWidget with: `bind(Layer*)` snapshots the current layer params; slider `Pressed` re-snapshots; slider `Released` / spin / line-edit `editingFinished` / combo & checkbox toggle emit `commitRequested(layer, before, after)` if the layer differs from the snapshot. The dock re-emits each pane's `previewChanged` (MainWindow → recomposite) and `<kind>CommitRequested` (MainWindow → push `LayerParamsCommand`).
- **GroupProperties (M6-S0).** Lives in `ui/PropertiesPaneGroup.h`: `{ string name; BlendMode blend; float opacity; bool clipToBelow; }`. The setter applies all four fields atomically — one undo entry per gesture covers the entire group-properties dialog. The blend combo carries the same 14-entry `kGroupBlendList` (Pass-Through first) that `LayerRowWidget` uses; switching to non-Pass-Through flips the group from leaky to isolated. `MainWindow::bindActiveAdjustmentToDock` dispatches `kind() == Group` → `bindGroup`; refresh after commit triggers `layersPanel_->refresh()` + canvas recomposite because Pass-Through ↔ isolated transitions reshuffle compose.

## 16. LayersPanel Drag-and-Drop (M6-S1)

- `src/ui/LayersPanel.cpp` defines a private `LayerListWidget : QListWidget` subclass (anon namespace) for the panel's embedded list. Item widgets (the `LayerRowWidget` overlays) eat the mouse events that `QListWidget`'s built-in drag detection would otherwise consume, so the subclass tracks press position in `mousePressEvent` and initiates a manual `QDrag` in `mouseMoveEvent` once the cursor crosses `QApplication::startDragDistance()`.
- **MIME wire format.** `application/x-tuxels-layerid` carrying an 8-byte little-endian `LayerId`. We never accept drops from outside the panel itself (`event->source() != this` → ignore).
- **Drop hit-testing.** Inside the target row's `visualItemRect`, three zones: top quarter = `Above`, bottom quarter = `Below`, middle half = `On`. `On` only matters for `GroupLayer` rows (drop INTO group at end of children); on non-group rows it collapses to `Below`.
- **Tree-coordinate translation.** Panel reverses tree order (`buildDisplayList` iterates children in `rbegin..rend`). Above target X (tree idx `K_X`) → final tree idx = `K_X`. Below → `max(0, K_X - 1)`. On group → `group->children.size()`. The "final" idx is the position the moved layer will occupy after the move; passing it directly to `LayerTree::move(fromP, fromI, toP, toI)` produces the right layout (vector::insert at idx N inserts BEFORE existing idx N, so post-erase + insert at K final yields the moved layer at K).
- **Cycle detection.** `LayersPanel::emitLayerDrop` walks the target parent's ancestor chain looking for the moved id; matches abort silently. Same-parent same-slot and adjacent-slot drops that resolve to the same position are filtered upstream of the LayerOpCommand push so the undo stack doesn't accumulate dead entries.
- **Command flow.** `LayersPanel::layerDroppedRequested(movedId, targetParentId, targetIndex)` → MainWindow lambda → `LayerOpCommand` whose closures call `doc.tree().move(fromP, fromI, toP, toI)`. Same-parent symmetric undo works because tree.move's post-erase frame applies in both directions; cross-parent undo is the obvious mirror move.

## 17. Multi-Select Layers (M6-S2)

- `Document::selectedLayerIds_` (vector<LayerId>) is independent of `activeLayerId_` (the anchor / last-clicked row). The panel maintains both. `setSelectedLayerIds(...)`, `selectedLayerIds() const`, `selectedLayers()` (resolves to currently-living layer pointers, filtering stale ids on the fly).
- **Panel binding.** `LayersPanel`'s embedded list switches to `QAbstractItemView::ExtendedSelection`. `itemSelectionChanged` syncs the row-set into `Document::setSelectedLayerIds`; `currentRowChanged` still drives `activeLayerId`. `refresh()` re-applies the multi-selection set onto the QListWidget so selections survive reorders + group expand/collapse + undo/redo.
- **Batch ops in MainWindow** (when `selectedLayerIds().size() > 1`):
  - **Delete** filters descendants of selected ancestors (so deleting a group doesn't double-delete its children — the unique_ptr ownership chain transitively destroys them as part of the parent's stash). Each survivor is removed in iteration order with a fresh `tree.locate(id)`; the (parentId, idx, ptr) tuple is captured at TIME OF removal (post-prior-removals). Undo reverses the stash and re-inserts at recorded slots — works because reverse undo replays in matching order so each insert lands in a then-current tree state where the recorded slot is valid.
  - **Group** walks `tree.flatten()` to collect filtered ids in bottom-up tree order. Each removal updates the captured "topmost slot"; the new GroupLayer lands at the last captured (parent, idx) so it occupies the topmost selected layer's position. Children land in bottom-up tree order. Undo pulls the group out, drops its children, and re-inserts each at recorded slot in reverse.
  - **Visibility** captures per-layer `oldVal` so undo restores mixed prior states correctly (PS-style: setting all to `newVal` may restore some to true and others to false on undo).
- **Tools stay single-active.** Move / Transform / Free Transform / Paint / Marquee etc. consume `activeLayer()` only for M6. Multi-active tool semantics (Move on bbox-union, Transform on bbox-union, etc.) are scoped out — see `docs/NEXT.md` M7 candidate #1.

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
