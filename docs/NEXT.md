# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**S7 — Brush engine + BrushTool**

Goal: click-drag paints a round brush stroke onto the active pixel layer's
`TuxImage`, with `[` / `]` adjusting size and `B` activating the brush tool.
Mouse events originate in `CanvasView` and must be forwarded to the current
tool; the tool owns the math.

Files to create:
- `/home/james/Tuxels/src/brush/RoundBrush.{h,cpp}` — brush parameters
  (`diameter`, `hardness ∈ [0,1]`, `opacity ∈ [0,1]`, `flow ∈ [0,1]`,
  `spacingRatio` default 0.1, `color : Rgba32F`). Precomputes a stamp
  kernel: radial falloff where `hardness = 1` → hard-edged, `hardness = 0` →
  smoothstep from 0 to diameter/2. Store as `std::vector<float>` sized
  `diameter × diameter`, regenerate when size/hardness change.
- `/home/james/Tuxels/src/brush/BrushEngine.{h,cpp}` — given a stroke path
  (sequence of points), lay down stamps spaced `diameter × spacingRatio`
  apart (min 1px). `applyStamp(TuxImage&, cx, cy)` writes into the target
  image using straight-alpha "brush over surface" compositing:
  `out = src.a*flow*kernel * color + (1 - src.a*flow*kernel) * surface`.
  (Per-pixel, no premultiplication — TuxImage stores straight-alpha floats.)
- `/home/james/Tuxels/src/tools/ToolBase.h` — minimal polymorphic base:
  `virtual void pressEvent(...)`, `moveEvent`, `releaseEvent`. Tools receive
  **image-space** coordinates, not widget-space — CanvasView converts.
- `/home/james/Tuxels/src/tools/BrushTool.{h,cpp}` — owns a `RoundBrush`
  and a `BrushEngine`. On press, begin stroke at active layer's image; on
  move, extend stroke; on release, finalize.

Integration:
- Add `Tool` enum to `MainWindow` or a small `ToolState` singleton; default = BrushTool.
- `CanvasView` keeps a `Tool*` pointer and forwards `mousePressEvent` etc.
  Convert widget→image coords via the existing zoom/pan state.
- Keyboard: `B` → activate brush; `[` / `]` decrement/increment `diameter`
  by max(1, diameter/10). Shortcuts live in `MainWindow`.
- After each brush stamp batch, call `canvas_->requestRecomposite()` (may
  want incremental recompose later; M0 full-recompose is fine).
- A tiny Tools dock or toolbar button isn't strictly required for M0; `B`
  keybind + brush-always-on is acceptable.

Tests:
- `tests/test_brush.cpp` — linkable against `tuxels_core` (brush & engine
  must stay Qt-free). Assertions:
  - Stamp at exact pixel boundary writes solid center pixel with full alpha
    when hardness=1, opacity=1, flow=1.
  - Spacing: a stroke from (0,0) to (100,0) with diameter=10, spacing=0.1
    produces ~100 stamps.
  - Falloff: at diameter/2 the kernel value is 0 (hardness≥0), and at
    center it is 1 (opacity/flow applied separately).

Commit: "tools: round brush + brush engine + B/[/] shortcuts".

## Then — S8: Undo/redo with tile COW

`src/history/{Command,UndoStack,PaintCommand,LayerOpCommand}.{h,cpp}`. Paint
commands snapshot `shared_ptr<Tile>` for each tile touched before the first
stamp; undo swaps back. Layer-op commands capture metadata (name, blend,
opacity, visibility, index). `Ctrl+Z` / `Ctrl+Shift+Z`.

## Future Steps (brief)

- S9: Layer-mask UI (Add Layer Mask action, mask thumb in LayerRowWidget,
  click-to-target, shift-click to disable).
- S10: Manual end-to-end verify of every DoD bullet + tag `v0.0.1-m0`.

See full detail in `/home/james/.claude/plans/modular-singing-teacup.md`.

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state.
2. This file — what to do next.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
4. `git log --oneline -10` — recent commits.
5. `cmake --build build && ctest --test-dir build` — confirm green tree
   (should be 47 passing tests as of S6).
6. Pick up "Immediately Next" above.
