# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M4 active.** S0 + S1 + S2 + S3 shipped 2026-04-26. 299 tests across
32 executables. Next is **M4-S4: Hue/Sat + Brightness/Contrast ports.**

S4 work (per `/home/james/.claude/plans/quirky-napping-koala.md`):

1. **`src/ui/PropertiesPaneHueSat.{h,cpp}`** (new) — slider-only pane
   for `HueSaturation`. Three sliders: hue (-180..+180°), saturation
   (-100..+100), lightness (-100..+100). Same press-snapshot /
   release-commit discipline as Levels. UI maps integer slider values
   to floats: hue is degrees as-is (or /1 since slider range is the
   same), sat/light divided by 100 for the float param. No histogram.
2. **`src/ui/PropertiesPaneBrightnessContrast.{h,cpp}`** (new) — same
   pattern. Two sliders: brightness (-100..+100), contrast
   (-100..+100). Both divided by 100 for the float param ∈ [-1, 1].
3. **`PropertiesDock`** extended with `bindHueSat(HueSaturation*)` and
   `bindBrightnessContrast(BrightnessContrast*)` (no histogram for
   either). New `hueSatCommitRequested` and
   `brightnessContrastCommitRequested` re-emit signals. Two new pages
   in the QStackedWidget.
4. **MainWindow refactor** — `onLayerAddHueSaturation` and
   `onLayerAddBrightnessContrast` follow the Levels/Curves pattern
   exactly (insert identity + push LayerOpCommand + bind dock +
   raise). `onEditAdjustmentRequested` H-S and B&C branches become
   2 lines each. `bindActiveAdjustmentToDock` extended for both.
   Two new commit lambdas in `buildDocks()`.
5. **Delete `src/ui/HueSatDialog.{h,cpp}` and `src/ui/BrightnessContrastDialog.{h,cpp}`** + CMakeLists entries.
6. **`tests/test_properties_pane_smaller.cpp`** — covers both panes:
   bind, slider drag, release → one commit each; bind to nullptr →
   empty state shown via `boundLayer() == nullptr`.

After S4: S5 (verify + tag v0.4.0-m4) — needs the user's DoD walkthrough.

Cold-start verification:

```
cmake --build build && ctest --test-dir build
QT_QPA_PLATFORM=offscreen timeout 2 ./build/tuxels
```

Expect 299 passing across 32 executables at S3.

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state (M4 active, S0–S3 done).
2. This file — S4 detailed instructions + step queue.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
4. `cat /home/james/.claude/plans/quirky-napping-koala.md` — full M4 plan.
5. `git log --oneline -20` + `git tag --list` — recent commits and
   shipped tags (`v0.0.1-m0`, `v0.1.0-m1`, `v0.2.0-m2`, `v0.3.0-m3`).
6. `cmake --build build && ctest --test-dir build` — confirm green
   tree (299 passing at M4-S3).
