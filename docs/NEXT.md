# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M3-S0 landed** (adjustment-layer infrastructure — 222 tests green,
+7 in `test_adjustment_layer`). Plan:
`/home/james/.claude/plans/rosy-wiggling-axolotl.md`.

**Start here: S1 — Histogram backbone.**

The goal of S1 is the pre-dialog scan that Levels (S2) and Curves (S3)
both use to render their histogram backdrops. Pure-C++ core, no Qt —
the dialog widget in S2 becomes the only consumer, and the computation
lives in `tuxels_core` so tests can exercise it without a display.

Concretely (in this order):

1. `src/core/Histogram.h` — declare
   `struct Histogram4x256 { uint32_t buckets[4][256]; uint64_t total; };`
   and
   `Histogram4x256 computeHistogram(const TuxImage& src,
   const SelectionMask* clip = nullptr);`.
   Channels 0/1/2 are R/G/B; channel 3 is luminance
   (`0.299*R + 0.587*G + 0.114*B`) so dialogs can show a composite
   curve without re-scanning.
2. `src/core/Histogram.cpp` — walk `src.tiles()` (present tiles only,
   sparse-safe), pixel via `TileRowCursor`. Skip `a <= 0`. For each
   present pixel, clamp each channel to `[0,1]`, multiply by 255,
   `std::lround` to `uint8_t`, increment the matching bucket;
   increment `total`. When `clip` is non-null, test
   `clip->sample(x,y) >= 0.5` (matches SelectionMask binary-threshold
   semantics used elsewhere) before counting the pixel.
3. `CMakeLists.txt` — add `src/core/Histogram.cpp` to
   `TUXELS_CORE_SOURCES`, register `test_histogram` via
   `tuxels_add_test`.
4. `tests/test_histogram.cpp` — cases:
   - Constant-red 32×32 yields `total == 1024`, `buckets[0][255] ==
     1024`, `buckets[1][0] == buckets[2][0] == 1024`, luminance
     bucket ≈ 76 (`0.299 * 255`).
   - Fully transparent image yields `total == 0` (all buckets zero).
   - Selection clip restricts the count to the clipped region
     (half-doc rect → `total == w * h / 2`).
   - Empty `TuxImage` (0×0) returns zeros without crashing.

Keep the 222-test floor green. Expect ~4–6 new cases in
`test_histogram`.

The callers that compose-below-then-bucket live in S2 (when the Levels
dialog opens). S1 only ships the histogram primitive + its tests.

## When S1 is Done

Commit as `core: Histogram4x256 + computeHistogram (M3-S1)`. Update
`docs/STATUS.md` S1 row to ✅ done with the new test count
(222 + new = expected ~226–228). Update `docs/NEXT.md` to point at
**S2 — Levels adjustment** (the first real adjustment type, owner of
`LevelsDialog` + `LayerParamsCommand`).

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state (M3 active, S0 ✅, S1 next).
2. This file — exact next actions for the current step.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
4. `cat /home/james/.claude/plans/rosy-wiggling-axolotl.md` — M3 plan
   (authoritative).
5. `cat /home/james/.claude/plans/cryptic-stargazing-moonbeam.md` — M2
   plan archive (reference only).
6. `git log --oneline -20` + `git tag --list` — recent commits and the
   `v0.2.0-m2` tag.
7. `cmake --build build && ctest --test-dir build` — confirm green
   tree (222 passing after S0; growing through M3).
