# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M3-S2 landed** (Levels adjustment + dialog + `LayerParamsCommand` — 236
tests green, +7 in `test_levels`). Plan:
`/home/james/.claude/plans/rosy-wiggling-axolotl.md`.

**Start here: S3 — Curves adjustment.**

S3 lands the second headline adjustment type and introduces monotone
Hermite spline interpolation for the curve LUT. It reuses most of the
S2 plumbing: the compose adjustment path, the auto-mask-on-create
affordance, the `LayerParamsCommand` template, and the
edit-on-thumbnail-click signal. The net new surface is a spline
helper, a per-channel curve record, and a curve-editor dialog widget.

Concretely (in this order):

1. `src/geom/Spline.{h,cpp}` — monotone cubic Hermite (the Fritsch-
   Carlson variant so the curve never overshoots). Input: sorted
   control points `(x, y)` ∈ `[0,1]²` (endpoints `(0,0)` and `(1,1)`
   implied if caller doesn't pin them). Output: `void
   buildLut256(const std::vector<QPointF>& pts, uint8_t* out)` — fills
   a 256-entry byte LUT the same way Levels caches its LUT. Pure C++
   in `tuxels_core` (QPointF is available without Qt widgets — it's in
   `QtCore`; if that turns out to drag in Qt headers into core, swap
   for a local `struct SplinePoint { float x, y; }` instead. Prefer
   the local struct — keeps `tuxels_core` Qt-free).

2. `src/layers/CurvesAdjustment.{h,cpp}` — subclass of
   `AdjustmentLayer`. Per-channel `std::vector<SplinePoint>`
   (`Composite / R / G / B`, identical enum to Levels since the dialog
   shape is the same). `setPoints(CurvesChannel, std::vector<...>)`
   rebuilds that channel's cached `uint8_t lut_[256]`. `applyToAccum`
   mirrors `LevelsAdjustment::applyToAccum`: composite LUT first,
   then per-channel LUT, alpha untouched. Default-constructed state
   is `[(0,0), (1,1)]` on every channel so identity is zero-cost.

3. `src/ui/CurvesDialog.{h,cpp}` — modal `QDialog`. Main widget is a
   256×256 `CurveEditor` custom `QWidget` that paints:
   - The active channel's histogram bars (faded) as backdrop.
   - The LUT curve sampled at 256 x-points, drawn in the channel's
     colour.
   - Draggable control point dots.
   Interactions:
   - Left-click in empty space → add a control point at cursor.
   - Drag a dot → move it (clamped to `[0,1]²`, x-sorted on release).
   - Right-click a dot → delete (no-op if <3 points would remain).
   Channel combo above the editor re-points at the relevant
   `std::vector<SplinePoint>`. OK commits via
   `LayerParamsCommand<CurvesAdjustment, std::array<
   std::vector<SplinePoint>, 4>>`; Cancel restores the snapshot.

4. `src/app/MainWindow.cpp` — wire the menu + re-edit path:
   - `Layer → New Adjustment Layer → Curves…` (Ctrl+M — matches PS,
     and our M2 audit showed Ctrl+M is unbound).
   - `onLayerAddCurves` mirrors `onLayerAddLevels`: compose → hist →
     `addAdjustmentLayer` → open dialog → OK wraps in
     `LayerOpCommand` / Cancel rolls back.
   - `onEditAdjustmentRequested` already handles dispatch — extend the
     `dynamic_cast` ladder to route `CurvesAdjustment*` through the
     curves dialog.

5. `CMakeLists.txt` — add `src/geom/Spline.cpp` +
   `src/layers/CurvesAdjustment.cpp` to `TUXELS_CORE_SOURCES`, add the
   dialog pair to the `tuxels` executable, register `test_curves`
   and `test_spline` via `tuxels_add_test`.

6. Tests:
   - `tests/test_spline.cpp` — identity `[(0,0),(1,1)]` produces
     `lut[i] == i`; midpoint pull `(0.5, 0.7)` brightens midtones;
     monotone-Hermite property check: `lut[i+1] >= lut[i]` for every
     monotone-increasing control-point set.
   - `tests/test_curves.cpp` — identity spline leaves composite
     unchanged; midtone lift brightens a 0.5-gray reference; per-
     channel R edit doesn't disturb G/B; mask restricts the effect.
     (Use the same test skeleton as `test_levels.cpp`.)

Keep the 236-test floor green. Expect ~8–10 new cases across
`test_spline` + `test_curves`.

## When S3 is Done

Commit as `adjustments: CurvesAdjustment + spline LUT + dialog
(M3-S3)`. Update `docs/STATUS.md` S3 row to ✅ done with the new test
count (236 + ~8–10 = expected ~244–246). Update `docs/NEXT.md` to
point at **S4 — `.txl` v3 round-trip** (bump format, kind ordinals,
per-kind descriptors).

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state (M3 active, S0–S2 ✅, S3 next).
2. This file — exact next actions for the current step.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
4. `cat /home/james/.claude/plans/rosy-wiggling-axolotl.md` — M3 plan
   (authoritative).
5. `cat /home/james/.claude/plans/cryptic-stargazing-moonbeam.md` — M2
   plan archive (reference only).
6. `git log --oneline -20` + `git tag --list` — recent commits and the
   `v0.2.0-m2` tag.
7. `cmake --build build && ctest --test-dir build` — confirm green
   tree (236 passing after S2; growing through M3).
