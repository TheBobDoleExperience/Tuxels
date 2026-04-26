# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M4 active.** S0 + S1 + S2 shipped 2026-04-26. 292 tests across
31 executables. Next is **M4-S3: Curves port.**

S3 work (per `/home/james/.claude/plans/quirky-napping-koala.md`):

1. **Lift `CurveEditor` out of `src/ui/CurvesDialog.cpp`** into a new
   reusable `src/ui/CurveEditor.{h,cpp}`. Currently it's a
   `Q_OBJECT` declared inline inside the dialog's .cpp via
   `#include "CurvesDialog.moc"`. Need to promote to a real header
   so PropertiesPaneCurves can host it.
2. **`src/ui/PropertiesPaneCurves.{h,cpp}`** (new) — bare `QWidget`
   mirroring `PropertiesPaneLevels`'s shape: channel combo + the
   lifted `CurveEditor` + histogram backdrop. Same snapshot/commit
   discipline. The editor commits on:
   - Mouse press = snapshot.
   - Drag = preview only.
   - Mouse release on a moved point = one commit.
   - Click adds a point = one commit (snapshot+commit bracketed in
     the editor's `mousePressEvent`).
   - Right-click removes = one commit.
   - Channel combo switch = no commit, just reload.
3. **`PropertiesDock::bindCurves(CurvesAdjustment*, Histogram4x256)`**
   — add a Curves pane page to the stacked widget; signal
   re-emit for `curvesCommitRequested`.
4. **MainWindow refactor** — same as S2. `onLayerAddCurves` becomes
   "insert identity + push LayerOpCommand + bind dock + raise".
   `onEditAdjustmentRequested` Curves branch becomes "bind dock +
   raise". `bindActiveAdjustmentToDock()` extends to dispatch on
   `CurvesAdjustment*`. Delete `src/ui/CurvesDialog.{h,cpp}` and
   the CMakeLists entry.
5. **`tests/test_properties_pane_curves.cpp`** — same shape as
   `test_properties_pane_levels`: bind, drag a control point →
   one commit; add a point → one commit; right-click remove → one
   commit; channel switch → no commit.

After S3: continue with S4 (H-S + B&C ports — both slider-only,
should be quick), then S5 (verify + tag v0.4.0-m4).

Cold-start verification:

```
cmake --build build && ctest --test-dir build
QT_QPA_PLATFORM=offscreen timeout 2 ./build/tuxels
```

Expect 292 passing across 31 executables at S2.

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state (M4 active, S0–S2 done).
2. This file — S3 detailed instructions + step queue.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
4. `cat /home/james/.claude/plans/quirky-napping-koala.md` — full M4 plan.
5. `git log --oneline -20` + `git tag --list` — recent commits and
   shipped tags (`v0.0.1-m0`, `v0.1.0-m1`, `v0.2.0-m2`, `v0.3.0-m3`).
6. `cmake --build build && ctest --test-dir build` — confirm green
   tree (292 passing at M4-S2).
