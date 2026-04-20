# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M2-S3 — Free Transform (Ctrl+T).** S0/S1/S2 landed 2026-04-20 (layer
origins + Place + Move). Next up: a modal transform tool covering scale
+ rotate + translate, committed through the existing tile-COW undo
pipeline so redo replays the final resampled pixels instead of
re-running the affine.

- `src/geom/Resample.{h,cpp}` (new): bilinear sample of a source
  `TuxImage` through an affine transform. Iterates destination pixels
  in doc coords, samples source in layer-local coords via
  `TileRowCursor`, respects the active selection if present.
- `src/tools/TransformTool.{h,cpp}` (new): owns a scratch `TuxImage`
  sized to the current transform's bbox. Handle drag re-resamples at
  view resolution while the user is adjusting; commit resamples at
  doc resolution into the real layer via
  `beginRecord/setPixel/stopRecord`.
- `src/compositor/compose.cpp`: add an overload accepting a
  `LayerOverride` keyed by layer id (substitute image + origin for the
  transformed layer during preview) — keeps the blend pipeline
  untouched and lines up with the future S2 live-preview path too.
- UI: 8 bbox handles (corners = scale, edges = axis-locked scale,
  outside bbox = rotate, inside bbox = translate), `Shift` constrains
  aspect or snaps rotation to 15°, `Enter`/double-click commits,
  `Escape`/right-click cancels, tool-switch while pending prompts.
- Commit path: `PaintCommand` (full tile-COW) plus a
  `MoveLayerCommand` (or a combined `TransformCommand`) for the origin
  delta. Reuses S2's id-based origin swap.
- Tests: `test_resample` (identity is bitwise, 90° round-trips, 2×
  scale spot checks), `test_transform_command` (undo restores pixels +
  origin).

After S3:

- S4 — Lasso + Polygonal Lasso; `SelectionMask::fillPolygon` scanline.
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
   tree (expect 151 passing tests as of M2-S2 ship).
7. Pick up "Immediately Next" above.
