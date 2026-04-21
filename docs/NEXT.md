# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M3-S1 landed** (histogram primitive — 229 tests green, +7 in
`test_histogram`). Plan: `/home/james/.claude/plans/rosy-wiggling-axolotl.md`.

**Start here: S2 — Levels adjustment.**

S2 is the first *real* adjustment type and the pattern every later
adjustment (Curves, Hue/Sat, Brightness/Contrast) will copy. It also
introduces the generic `LayerParamsCommand` the rest of M3 reuses, and
the dialog-driven commit flow that routes through
`MainWindow::onEditAdjustmentRequested`.

Concretely (in this order):

1. `src/layers/LevelsAdjustment.{h,cpp}` — subclass of
   `AdjustmentLayer`. Params:
   ```cpp
   struct LevelsParams {
     float inBlack = 0.f, inWhite = 1.f, gamma = 1.f;
     float outBlack = 0.f, outWhite = 1.f;
   };
   enum class LevelsChannel { Composite = 0, R = 1, G = 2, B = 3 };
   ```
   Hold `LevelsParams params_[4]` (one per channel, `[0]` is composite).
   `setParams(LevelsChannel, const LevelsParams&)` rebuilds a cached
   256-entry `uint8_t lut_[4][256]` using the standard
   `in → clamp((in-inB)/(inW-inB), 0, 1) → pow(_, 1/gamma) →
   outB + _*(outW-outB) → clamp → round to byte` pipeline. Identity
   params must produce identity LUT (`lut[i] == i`).
   `applyToAccum(tc, accum)`: per pixel, apply composite LUT to R/G/B
   straight (matches PS composite behavior — a single param set acts on
   each channel identically), then the per-channel LUT. Alpha is
   untouched.

2. `src/history/LayerParamsCommand.{h,cpp}` — generic templated
   command. Signature:
   ```cpp
   template <class L, class P>
   class LayerParamsCommand : public Command {
    public:
     LayerParamsCommand(L* layer, P before, P after,
                        std::function<void(L*, const P&)> set);
     void apply() override;  // set(layer, after)
     void undo() override;   // set(layer, before)
   };
   ```
   The `set` callback is the only per-layer-type point of variation —
   Levels' setter calls `setParams` for every channel; Curves' setter
   swaps the `std::vector<QPointF>` arrays; Hue/Sat just assigns the
   triple. Reused identically by S3 / S6.

3. `src/ui/LevelsDialog.{h,cpp}` — modal `QDialog`. Layout:
   - Channel selector `QComboBox` (Composite/R/G/B).
   - Histogram backdrop `QWidget` painted from a `Histogram4x256` passed
     in at construction (bucket bars, tallest bucket = full height).
   - Input black / gamma / white sliders + `QDoubleSpinBox` numeric
     edits.
   - Output black / white sliders.
   - OK / Cancel buttons.
   Live preview: every slider edit calls
   `layer->setParams(activeChannel, currentParams)` and emits
   `parametersChanged` so MainWindow requests a recomposite. OK commits
   a `LayerParamsCommand` with the snapshotted-on-open params as the
   before; Cancel restores before via the same setter.

4. `src/app/MainWindow.{h,cpp}` — wire the menu + dialog launcher.
   - `Layer → New Adjustment Layer → Levels…` (Ctrl+L — matches PS,
     currently unused per existing shortcut audit).
   - Handler: build a `LevelsAdjustment`, compose the doc as-is into a
     temp `TuxImage`, compute a `Histogram4x256` from it, open
     `LevelsDialog` with (layer*, histogram) — the layer isn't inserted
     yet. On OK: `document_->addAdjustmentLayer(std::move(layer))` +
     push the create-adjustment command onto the undo stack. On Cancel:
     discard.
   - `onEditAdjustmentRequested(LayerBase*)` (stubbed in S0): if the
     layer is `LevelsAdjustment`, snapshot current params, open
     `LevelsDialog` seeded with them; OK commits `LayerParamsCommand`.

5. `CMakeLists.txt` — add the four new `.cpp` files to the appropriate
   target (`tuxels_core` for LevelsAdjustment + LayerParamsCommand; the
   `tuxels` executable for LevelsDialog). Register `test_levels`.

6. `tests/test_levels.cpp` — cases (core-only, no Qt):
   - Identity params (`inB=0, inW=1, gamma=1, outB=0, outW=1` on every
     channel) leave a constant-red image unchanged (±0 bytes after
     round-trip through LUT → float).
   - `gamma = 2.2` on composite brightens a 0.5-gray reference by the
     analytical amount (±1/255).
   - `inBlack = 0.5` on composite clips the lower half of RGB to 0.
   - Per-channel override: red channel Levels doesn't disturb G/B.
   - Mask region restricts the effect (paint mask to 0 in half the doc
     → that half is unchanged).
   - Round-trip through `applyToAccum` + inverse Levels is bit-exact
     identity (sanity check).

Keep the 229-test floor green. Expect ~6 new cases in `test_levels`.

## When S2 is Done

Commit as `adjustments: LevelsAdjustment + dialog + LayerParamsCommand
(M3-S2)`. Update `docs/STATUS.md` S2 row to ✅ done with the new test
count (229 + ~6 = expected ~235). Update `docs/NEXT.md` to point at
**S3 — Curves adjustment** (Hermite spline + curve editor widget).

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state (M3 active, S0–S1 ✅, S2 next).
2. This file — exact next actions for the current step.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
4. `cat /home/james/.claude/plans/rosy-wiggling-axolotl.md` — M3 plan
   (authoritative).
5. `cat /home/james/.claude/plans/cryptic-stargazing-moonbeam.md` — M2
   plan archive (reference only).
6. `git log --oneline -20` + `git tag --list` — recent commits and the
   `v0.2.0-m2` tag.
7. `cmake --build build && ctest --test-dir build` — confirm green
   tree (229 passing after S1; growing through M3).
