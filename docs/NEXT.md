# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M3-S5 landed** (Free-Transform polish trio — Shift aspect-lock, Shift 15°
rotation snap, movable pivot; `DragMode::Pivot` + pivot-aware
`buildSrcToDoc`; 258 tests green, +4 in `test_transform_tool`). Plan:
`/home/james/.claude/plans/rosy-wiggling-axolotl.md`.

**Start here: S6 — Bonus adjustments (Hue/Saturation + Brightness/Contrast).**

Cheap follow-ons to S0–S3 now that `AdjustmentLayer` + `LayerParamsCommand`
+ `.txl` v3 kind dispatch (with reserved ordinals `4 = HueSaturation`,
`5 = BrightnessContrast`) all exist. Each adjustment is a new `.{h,cpp}`
pair in `src/layers/` + a matching modal dialog in `src/ui/` + menu wiring
in `MainWindow` + `TxlIO` activation of the reserved kind ordinal + a
unit-test file.

Concretely (in this order):

1. **Brightness/Contrast adjustment** (simpler, do it first).
   - `src/layers/BrightnessContrast.{h,cpp}`: subclass of
     `AdjustmentLayer`. `struct BrightnessContrastParams { float
     brightness = 0.f; float contrast = 0.f; };` (both ∈ `[-1, 1]`, 0 =
     identity). Cached 256-entry `uint8_t lut_[256]` rebuilt via
     `clamp((v − 0.5) · (1 + contrast) + 0.5 + brightness)` on
     `setParams`. `applyToAccum` runs the single LUT on R/G/B; alpha
     untouched.
   - `src/ui/BrightnessContrastDialog.{h,cpp}`: two slider+spinbox rows
     (Brightness / Contrast), live-preview via `previewChanged`,
     snapshot-on-open + `reject()` restore for Cancel.
   - `MainWindow::onLayerAddBrightnessContrast` + menu entry
     `Layer → New Adjustment Layer → Brightness/Contrast…` (no default
     PS shortcut — skip accelerator). Mirrors `onLayerAddLevels` —
     insert identity layer, open dialog, Accept wraps in
     `LayerOpCommand`; Cancel rolls back. Re-edit on "fx" click commits
     `LayerParamsCommand<BrightnessContrast, BrightnessContrastParams>`.
   - `src/io/TxlIO.cpp` activates kind ordinal `5` — descriptor is 2
     floats (`brightness`, `contrast`). Bump the `dynamic_cast` ladder
     in `kindByte()` to recognize `BrightnessContrast*`; add a read +
     write branch; extend `acceptsAdjustmentKinds` (already gated on
     `version >= 3`, so no version bump — v3 files that embed a B&C
     layer become writeable as soon as the kind is active).

2. **Hue/Saturation adjustment** (master only for M3; per-color-range
   editing is later).
   - `src/layers/HueSaturation.{h,cpp}`: `struct HueSaturationParams {
     float hueShift = 0.f; float saturation = 0.f; float lightness =
     0.f; };` — hue in degrees `[-180, 180]`, saturation/lightness in
     `[-1, 1]`. No LUT — the transform is per-pixel RGB↔HSL (HSL is
     what PS uses for Master, not HSV). `applyToAccum` converts each
     pixel to HSL, shifts hue (wraps mod 360), scales saturation (`s *=
     (1 + saturation)` clamped to `[0,1]`), adjusts lightness
     (`l += lightness` clamped), converts back to RGB. Alpha untouched.
     Keep the RGB↔HSL helpers local to the `.cpp` (no new utility
     module).
   - `src/ui/HueSatDialog.{h,cpp}`: three slider+spinbox rows (Hue /
     Saturation / Lightness), same live-preview + snapshot-restore
     shape as Levels/Curves/B&C.
   - `MainWindow::onLayerAddHueSaturation` + menu entry
     `Layer → New Adjustment Layer → Hue/Saturation…` (Ctrl+U — PS
     default, verify unused first).
   - `TxlIO.cpp` activates kind ordinal `4` — descriptor is 3 floats.

3. `tests/test_brightness_contrast.cpp` + `tests/test_hue_saturation.cpp`:
   - Identity params (0/0 for B&C, 0/0/0 for Hue/Sat) produce
     bit-close composites.
   - Mid-range params produce expected per-channel deltas (e.g. pure-red
     input with hueShift=120° becomes ~pure-green; saturation=-1 on
     saturated input desaturates toward luminance).
   - Half-doc mask restricts the effect.
   - `LayerParamsCommand` apply/undo/redo round-trips params correctly.
   - `.txl` round-trip for both kinds (extend `test_txl_io.cpp` with
     one HueSat case + one B&C case).

4. `CMakeLists.txt` — register `test_hue_saturation` +
   `test_brightness_contrast` via `tuxels_add_test` (each is a new
   file).

Keep the 258-test floor green. Expect ~8–12 new cases across the two
adjustment test files + two `.txl` extensions.

## When S6 is Done

Commit as `adjust: Hue/Saturation + Brightness/Contrast (M3-S6)`. Update
`docs/STATUS.md` S6 row to ✅ done with the new test count (258 +
~8–12 = ~266–270). Update `docs/NEXT.md` to point at **S7 — Verify +
tag `v0.3.0-m3`** (user walkthrough then tag).

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state (M3 active, S0–S5 ✅, S6 next).
2. This file — exact next actions for the current step.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
4. `cat /home/james/.claude/plans/rosy-wiggling-axolotl.md` — M3 plan
   (authoritative).
5. `cat /home/james/.claude/plans/cryptic-stargazing-moonbeam.md` — M2
   plan archive (reference only).
6. `git log --oneline -20` + `git tag --list` — recent commits and the
   `v0.2.0-m2` tag.
7. `cmake --build build && ctest --test-dir build` — confirm green
   tree (258 passing after S5; growing through M3).
