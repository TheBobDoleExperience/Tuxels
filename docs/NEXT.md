# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M3-S3 landed** (Curves adjustment + spline LUT + dialog — 248 tests
green, +7 in `test_spline`, +5 in `test_curves`). Plan:
`/home/james/.claude/plans/rosy-wiggling-axolotl.md`.

**Start here: S4 — `.txl` v3 round-trip.**

S4 bumps the native format so adjustment layers survive save/open.
Today's `TxlIO` only knows how to serialize `PixelLayer` (image +
mask). It needs to dispatch on a new per-layer `kind` ordinal so
adjustment layers round-trip with their params.

Concretely (in this order):

1. `src/io/TxlIO.{h,cpp}` — bump `kVersionCurrent` to `3`. Add a layer
   `kind` ordinal, written once per layer right after the existing
   per-layer header:
   - `1` — `PixelLayer` (current behaviour)
   - `2` — `LevelsAdjustment`
   - `3` — `CurvesAdjustment`
   - Reserve `4` — `HueSaturation`, `5` — `BrightnessContrast` for S6.
   Per-kind body:
   - **Pixel (1):** existing flow unchanged (`LayerWidth/Height/
     OriginX/OriginY + NameLen + Name + Image + Mask`).
   - **Adjustment (2/3):** `LayerWidth = LayerHeight = 0`,
     `OriginX = OriginY = 0`, then `NameLen + Name`, then
     `NumImageTiles = 0` (kept for shape uniformity), then a
     kind-specific descriptor, then mask tiles via the existing flow.
     - Levels descriptor: 4 channels × 5 floats = 80 bytes
       (`inBlack / inWhite / gamma / outBlack / outWhite` per
       Composite / R / G / B).
     - Curves descriptor: per channel `numPoints: u32` + `numPoints`
       `{ x: f32, y: f32 }` pairs, repeated for Composite / R / G / B.
2. **Reader:** keep v1 and v2 readers exactly as they are (assume
   kind = Pixel when the version predates kind ordinals). v3 reader
   dispatches on kind, calls the matching kind's reader to populate
   a freshly constructed `LevelsAdjustment` / `CurvesAdjustment`,
   then runs the existing mask-attachment path. For unknown kinds
   (future-proofing), read the descriptor size-prefixed if feasible
   or bail with a clean error — prefer bail in v3 since we haven't
   reserved a size prefix yet.
3. **Writer:** always emit v3. When serializing, inspect
   `layer->kind()` and `dynamic_cast` to the concrete type to pull
   the descriptor.
4. `tests/test_txl_io.cpp` — extend with round-trip cases:
   - Doc with a `LevelsAdjustment` layer + mask: params round-trip
     bit-exact, mask tiles round-trip, kind survives.
   - Doc with a `CurvesAdjustment` layer carrying multiple control
     points per channel.
   - Mixed doc (pixel + levels + curves) preserves ordering.
   - v2 file loads cleanly into a v3-aware build (backward-compat
     regression).
5. `CMakeLists.txt` — no changes needed (TxlIO already linked).

Keep the 248-test floor green. Expect ~4–6 new cases in
`test_txl_io`.

## When S4 is Done

Commit as `io: .txl v3 with adjustment-layer kinds (M3-S4)`. Update
`docs/STATUS.md` S4 row to ✅ done with the new test count (248 +
~4–6 = expected ~252–254). Update `docs/NEXT.md` to point at
**S5 — Free-Transform polish trio** (Shift aspect-lock, 15° rotation
snap, movable pivot).

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state (M3 active, S0–S3 ✅, S4 next).
2. This file — exact next actions for the current step.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
4. `cat /home/james/.claude/plans/rosy-wiggling-axolotl.md` — M3 plan
   (authoritative).
5. `cat /home/james/.claude/plans/cryptic-stargazing-moonbeam.md` — M2
   plan archive (reference only).
6. `git log --oneline -20` + `git tag --list` — recent commits and the
   `v0.2.0-m2` tag.
7. `cmake --build build && ctest --test-dir build` — confirm green
   tree (248 passing after S3; growing through M3).
