# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M2-S4 — Lasso + Polygonal Lasso.** S0/S1/S2/S3 landed 2026-04-20
(layer origins + Place + Move + Free Transform). Next up: two freehand
selection tools that share a tile-aware polygon rasterizer.

- `src/core/SelectionMask.{h,cpp}`: add
  `fillPolygon(const std::vector<QPointF>& pts, float value, SelectionMode combineMode)`.
  Tile-aware scanline rasterizer: compute polygon bbox → candidate
  tile range; build active-edge table once per tile's y-range; write
  runs directly into tile `data()` via `getOrCreate(tc)`. Even-odd
  parity so self-intersecting lassos behave. Fully-inside tiles get
  one `fill(1)`.
- `src/tools/ToolBase.h`: add
  `virtual std::optional<std::vector<QPointF>> livePath() const { return std::nullopt; }`
  so CanvasView can render the in-progress polyline (same marching-
  ants black-underlay + white-dashed-overlay style).
- `src/tools/LassoTool.{h,cpp}` (new): freehand. Accumulate points on
  `move`, decimate with 1-doc-pixel chord threshold, close polyline on
  release, rasterize via `fillPolygon`, commit via `SelectionCommand`
  with the same combine-mode + persistent-mode plumbing the marquee /
  wand already share.
- `src/tools/PolyLassoTool.{h,cpp}` (new): accumulate on press, live-
  drag the last edge on move, close on double-click or Enter. Escape
  cancels.
- `src/ui/ToolsPanel.{h,cpp}`: L (Lasso) picker button. Combine-mode
  options row (Replace / + / − / ∩) shared idiom with marquee.
  Shift-L cycles to Polygonal (follow-up — for M2 a separate picker
  button is acceptable if cycling is awkward).
- `src/ui/CanvasView.cpp`: paint `livePath()` as black-underlay +
  white-dashed polyline, matching marching-ants.
- Tests: `test_polygon_fill` (scanline correctness on concave + self-
  intersecting paths, tile-boundary crossing cases),
  `test_lasso_tool` (tool-level press/move/release → final mask).

After S4:

- S5 — Select by Color; Shift-W cycles from Wand.
- S6 — Brush dynamics (size / opacity jitter, spacing).
- S7 — Verify + tag `v0.2.0-m2`.

Expected deliverables per step (mirrors M1 shape):

1. Shipping UI wiring with menu + shortcut.
2. Unit tests for the headless / pure-C++ pieces (lives in
   `tuxels_core` when possible).
3. STATUS / NEXT / ARCHITECTURE doc updates.
4. A user-verification step at the end of the milestone before
   tagging `v0.2.0-m2`.

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state.
2. This file — what to do next.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
4. `cat /home/james/.claude/plans/cryptic-stargazing-moonbeam.md` — M2
   plan (authoritative).
5. `git log --oneline -10` — recent commits.
6. `cmake --build build && ctest --test-dir build` — confirm green
   tree (expect 176 passing tests as of M2-S3 ship).
7. Pick up "Immediately Next" above.
