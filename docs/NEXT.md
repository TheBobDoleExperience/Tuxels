# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M3-S4 landed** (`.txl` v3 with adjustment-layer kinds — 254 tests
green, +6 in `test_txl_io`). Plan:
`/home/james/.claude/plans/rosy-wiggling-axolotl.md`.

**Start here: S5 — Free-Transform polish trio.**

Three small `TransformTool` additions deferred from M2-S3 that each
match Photoshop's expected modifier behaviour. None of them touch the
adjustment-layer pipeline — S5 is purely a tool-UX step.

Concretely (in this order):

1. **Shift aspect-lock (Scale).** In
   `src/tools/TransformTool.cpp::move` `case DragMode::Scale`, when the
   incoming modifier mask carries `Qt::ShiftModifier`, lock `scaleY` to
   match `|scaleX|` (preserve the sign per corner so flips still work).
   Plumb modifiers in via the existing `ToolBase::setModifiers` hook —
   CanvasView already pushes modifier state per event for the marquee,
   so the wiring is a one-line consumer read. `rebuildScratch()` picks
   up the locked ratio for free.

2. **15° rotation snap (Rotate).** In `case DragMode::Rotate`, when
   Shift is held, snap `angle_` to the nearest multiple of `M_PI / 12`
   (15°). Snap the **delta**, not the absolute angle — a user who
   started at a non-multiple should stay consistent through the drag.

3. **Movable pivot.** Add `pivotX_` / `pivotY_` state to
   `TransformTool` (defaults to bbox center on `enter`). New
   `DragMode::Pivot` that fires when a press hit-tests within
   `kPivotHitRadius = 8.f` doc-px of `(pivotX_, pivotY_)` — moves the
   pivot only, no scale/rotate. `rebuildScratch()` changes to use the
   pivot as the rotation + scale center instead of
   `(centerX_, centerY_)`. Translate drag is unchanged (pivot tracks
   along with the whole transform). Render the pivot as a dot via a
   new `pivot` field on `Overlay` — `CanvasView::paintEvent` paints it
   as a crosshair / circle.

4. `tests/test_transform_polish.cpp` (new, or extend
   `test_transform_tool.cpp`):
   - Shift+Scale locks `scaleY = scaleX` magnitude, preserves sign.
   - Shift+Rotate snaps `angle_` to `k·π/12`.
   - Pivot drag moves `pivotX_/Y_` without mutating scale/rotate state,
     and a subsequent rotate pivots around the new center (verify by
     checking bbox corner positions post-rotation relative to the new
     pivot).

5. `CMakeLists.txt` — register `test_transform_polish` via
   `tuxels_add_test` if it lives in a new file.

Keep the 254-test floor green. Expect ~3–5 new cases.

## When S5 is Done

Commit as `transform: Shift aspect-lock + rotation snap + movable
pivot (M3-S5)`. Update `docs/STATUS.md` S5 row to ✅ done with the new
test count (254 + ~3–5 = expected ~257–259). Update `docs/NEXT.md` to
point at **S6 — Bonus adjustments (Hue/Saturation +
Brightness/Contrast)**.

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state (M3 active, S0–S4 ✅, S5 next).
2. This file — exact next actions for the current step.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
4. `cat /home/james/.claude/plans/rosy-wiggling-axolotl.md` — M3 plan
   (authoritative).
5. `cat /home/james/.claude/plans/cryptic-stargazing-moonbeam.md` — M2
   plan archive (reference only).
6. `git log --oneline -20` + `git tag --list` — recent commits and the
   `v0.2.0-m2` tag.
7. `cmake --build build && ctest --test-dir build` — confirm green
   tree (254 passing after S4; growing through M3).
