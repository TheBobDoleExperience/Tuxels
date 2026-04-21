# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M3 opened.** Plan approved 2026-04-21:
`/home/james/.claude/plans/rosy-wiggling-axolotl.md`. M3 delivers
adjustment-layer infrastructure + Levels + Curves (with static
histogram backdrop) + `.txl` v3 + three Free-Transform polish items +
bonus Hue/Saturation + Brightness/Contrast. Tasks #1–#8 mirror S0–S7.

**Start here: S0 — Adjustment-layer infrastructure.**

The goal of S0 is the smallest plumbing that lets an adjustment layer
affect the composite: add `LayerKind` discrimination on `LayerBase`,
introduce an abstract `AdjustmentLayer` subclass with an
`applyToAccum(TileCoord, Rgba32F* accum)` virtual, teach `compose()` to
dispatch on kind (pixel = existing blend-over path; adjustment =
transform-then-mask-blend-back path), give `Document` a
`addAdjustmentLayer<T>()` factory that auto-attaches a doc-sized white
mask, update `LayerRowWidget` to render adjustment thumbnails +
emit an edit-on-click signal, and exercise the whole pipe with a
test-only `InvertAdjustment` in `test_adjustment_layer`.

Concretely (in this order):

1. `src/layers/LayerBase.h` — add `enum class LayerKind { Pixel,
   Adjustment }` + `virtual LayerKind kind() const { return
   LayerKind::Pixel; }`. `PixelLayer` inherits the default.
2. `src/layers/AdjustmentLayer.{h,cpp}` — abstract subclass. Returns
   `LayerKind::Adjustment`. `renderTile` is `final` (returns false).
   Pure-virtual `applyToAccum(TileCoord, Rgba32F* accum) const`. No
   backing image (masks are doc-sized, owned via the existing
   `LayerBase::mask`).
3. `src/compositor/compose.cpp` — in `composeTileRange`, branch on
   `kind()`. Pixel: unchanged. Adjustment: snapshot the pre-adjustment
   `accum` into a scratch buffer (same size as the tile), call
   `adj->applyToAccum(tc, accumCopy)`, then for each pixel blend the
   transformed value back over the original using
   `factor = mask.r * opacity` (falls through to straight lerp when
   mask is absent — `mask == nullptr` treats it as uniform 1.0). A
   0-alpha or fully-transparent `accum` pixel is left alone (no
   sampling cost).
4. `src/core/Document.h` — `template <class T> T*
   addAdjustmentLayer(std::unique_ptr<T> layer)`: assigns id, appends
   to top, sets active, auto-attaches a doc-sized white `LayerMask`
   (`enabled = true`). Sets `paintTarget` to `Mask` so follow-up
   brush strokes land on the mask by default. Returns raw `T*` for
   subclass-specific follow-up (parameter setters, etc.).
5. `src/ui/LayerRowWidget.{h,cpp}` — in `rebuildThumbnail()`, check
   `layer->kind()`. For adjustment layers, render a fixed glyph (start
   with a simple 2-letter monochrome label on a dark background —
   "Lv" / "Cv" keyed off `dynamic_cast`, or a generic "fx" stub for
   unknown types — upgrade to a proper icon later). Adjustment-layer
   mask thumbs use the existing mask renderer unchanged. Add
   `signal editAdjustmentRequested(LayerBase*)` that fires on left-
   click of an adjustment-layer thumb (replaces paint-target swap,
   which doesn't apply). `LayersPanel` forwards the signal upward.
6. `src/app/MainWindow.cpp` — handle `editAdjustmentRequested` with a
   stub that does nothing for now (S2 wires the dispatch to
   `LevelsDialog`).
7. `tests/test_adjustment_layer.cpp` — create a test-only
   `InvertAdjustment` (negates RGB, leaves A). Confirm: full-opacity
   inversion flips pixel colors; 0.5 opacity mid-blends; doc-sized mask
   painted in one quadrant confines the inversion to that quadrant.

Keep the 215-test floor green. Expect ~6–8 new cases in
`test_adjustment_layer`.

## When S0 is Done

Commit as `adjustments: AdjustmentLayer + compose kind-dispatch
(M3-S0)`. Update `docs/STATUS.md` S0 row to ✅ done with the new test
count. Update `docs/NEXT.md` to point at **S1 — Histogram backbone**
(the pre-dialog scan used by Levels + Curves to render the histogram
backdrop; see the plan file for the exact shape).

Mark task #1 completed via `TaskUpdate`; `TaskList` will surface #2
(S1 — Histogram backbone) as the next claim.

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state (M3 active, S0 or later in
   progress).
2. This file — exact next actions for the current step.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
4. `cat /home/james/.claude/plans/rosy-wiggling-axolotl.md` — M3 plan
   (authoritative).
5. `cat /home/james/.claude/plans/cryptic-stargazing-moonbeam.md` — M2
   plan archive (reference only).
6. `git log --oneline -20` + `git tag --list` — recent commits and the
   `v0.2.0-m2` tag.
7. `cmake --build build && ctest --test-dir build` — confirm green
   tree (215 passing at M2 ship, growing through M3).
8. `TaskList` — pick up the lowest-numbered pending M3 task (#1–#8).
