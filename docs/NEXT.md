# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M3-S6 landed** (B&C + Hue/Saturation adjustments — new layer types +
modal dialogs + MainWindow wiring + `.txl` v3 kind-ordinals 4/5
activated; 274 tests green, +16 across new `test_brightness_contrast`,
new `test_hue_saturation`, and two `test_txl_io` extensions). Plan:
`/home/james/.claude/plans/floofy-spinning-sedgewick.md` (M3), with M2
archive at `/home/james/.claude/plans/cryptic-stargazing-moonbeam.md`.

**Start here: S7 — Verify + tag `v0.3.0-m3`.**

S7 is a user walkthrough on a real multi-layer doc, then the tag. The
unit tests already cover the correctness floor; S7 catches regressions
that only surface in interactive flow (dialog live-preview + menu
shortcuts + re-edit paths + cross-adjustment ordering).

Run the cold-start build + test pass first:

```
cmake --build build && ctest --test-dir build
QT_QPA_PLATFORM=offscreen timeout 2 ./build/tuxels
```

Then walk through each of these paths on a multi-layer doc (two pixel
layers + a mask on one is enough to shake things out):

1. Open a `.txl` or PNG. `Layer → New Adjustment Layer → Levels…`.
   Verify histogram backdrop renders; drag gamma → composite updates
   live; OK commits, Cancel rolls back. Single-click the adjustment
   row's left thumb → dialog re-opens; change params; OK pushes an
   `Edit Levels` undo entry.
2. Repeat with Curves (control-point add / drag / channel switch).
3. **S6 new — Brightness/Contrast**: `Layer → New Adjustment Layer →
   Brightness/Contrast…`. Drag each slider; preview updates live; OK
   commits, Cancel rolls back. Re-edit from the "fx" thumbnail pushes
   an `Edit Brightness/Contrast` undo entry.
4. **S6 new — Hue/Saturation (Ctrl+U)**: verify hue spins through the
   wheel (try ±120° on a saturated region); saturation = −1 drops
   toward grayscale; lightness bright/dark clamp cleanly. Re-edit
   from the "fx" thumbnail pushes an `Edit Hue/Saturation` undo entry.
5. Toggle visibility on each adjustment → composite reverts / reapplies.
6. Paint on the auto-attached mask → effect restricted to painted
   region.
7. Drag layer opacity → effect blends proportionally.
8. `File → Save As…` → `File → Open…` round-trips all four adjustment
   kinds + masks + ordering.
9. Undo-redo sequence through every add/edit.
10. Free Transform: Shift+Scale, Shift+Rotate, pivot drag all still
    work (regression check from S5).

Land any follow-up fixes inline as part of S7. Once the user signs off:

```
git tag -a v0.3.0-m3 -m "M3 — Non-destructive Adjustments (Levels, Curves, B&C, Hue/Sat)"
git push && git push --tags
```

Then update STATUS.md's "Active Milestone" line to mark M3 ✅ shipped,
and point NEXT.md at M4 kickoff (adjustment layer "clip to layer
below", adjustment-layer Properties dock, smart objects — scope TBD).

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state (M3 active, S0–S6 ✅, S7 next).
2. This file — exact next actions for S7.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
4. `cat /home/james/.claude/plans/floofy-spinning-sedgewick.md` — M3 plan
   (authoritative).
5. `cat /home/james/.claude/plans/cryptic-stargazing-moonbeam.md` — M2
   plan archive (reference only).
6. `git log --oneline -20` + `git tag --list` — recent commits + the
   `v0.2.0-m2` tag (M3 tag `v0.3.0-m3` lands at S7 sign-off).
7. `cmake --build build && ctest --test-dir build` — confirm green
   tree (274 passing after S6).
