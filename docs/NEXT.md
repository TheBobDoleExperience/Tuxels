# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M4 active.** S0 (ToolsPanel accordion shell) shipped 2026-04-26 (commit
TBA after this turn). 277 tests across 29 executables. Next is
**M4-S1: Adjustment-layer clip-to-layer flag.**

S1 work (per `/home/james/.claude/plans/quirky-napping-koala.md`):

1. **`src/layers/LayerBase.h`** — add `bool clipToBelow = false;`. Field
   on the base so future PixelLayer clipping can reuse it; M4 only
   honors the flag for adjustment layers.
2. **`src/compositor/compose.cpp`** — in `composeTileRange`, capture the
   most recent non-clipped pixel layer's per-pixel source alpha into a
   `lastBaseAlpha[kTilePixels]` buffer + `hasBase` flag. When the
   adjustment branch sees `layer->clipToBelow && hasBase`, multiply the
   per-pixel lerp factor `f *= lastBaseAlpha[idx]`. Clipped adjustment
   with `!hasBase` = skip entirely (PS-equivalent). Unclipped
   adjustments do NOT update `lastBaseAlpha`; only pixel layers do.
3. **`src/io/TxlIO.{h,cpp}`** — bump `kVersionCurrent = 4`. v4 layer
   record adds `ClipToBelow : uint8` after `HasMask` and before
   `Opacity`. v1/v2/v3 readers keep working (load with
   `clipToBelow == false`). Update format-comment header.
4. **`src/ui/LayerRowWidget.{h,cpp}`** — when `bindToLayer` sees
   `clipToBelow == true`, indent the row (~16 px left margin) and show
   a right-arrow glyph (`↳`) before the thumbnail. Hidden by default.
5. **`src/app/MainWindow.{h,cpp}`** — new `Layer → Create Clipping
   Mask` action with `Ctrl+Alt+G` shortcut. Slot toggles
   `clipToBelow` on the active layer via a `LayerOpCommand` (label
   `"Create Clipping Mask"` / `"Release Clipping Mask"` based on
   direction). No-op when no active layer or active layer is the
   bottom layer.
6. **Right-click context menu on `LayerRowWidget`** — extend the
   existing right-click pattern (currently mask-thumb-only) to add
   the same toggle entry on the layer row itself.
7. **Tests:**
   - `tests/test_clip_to_below.cpp` — three compose scenarios:
     (a) red bg + half-alpha green sprite + clipped Levels-invert →
         invert confined to green's alpha (red bg unchanged).
     (b) two adjacent clipped Levels over one green base → both
         gated by green's alpha.
     (c) clipped adjustment with no preceding pixel layer = no-op.
   - `tests/test_txl_io.cpp` extension — v4 round-trip with mixed
     clipped/unclipped layers; hand-crafted v3 file still loads with
     `clipToBelow == false` for all layers (back-compat regression).

After S1: continue with S2 (Properties dock + Levels port), then
S3 (Curves port), then S4 (H-S + B&C ports), then S5 (verify + tag).

Cold-start verification:

```
cmake --build build && ctest --test-dir build
QT_QPA_PLATFORM=offscreen timeout 2 ./build/tuxels
```

Expect 277 passing across 29 executables at S0.

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state (M4 active, S0 done).
2. This file — S1 detailed instructions + step queue.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
4. `cat /home/james/.claude/plans/quirky-napping-koala.md` — full M4 plan.
5. `git log --oneline -20` + `git tag --list` — recent commits and
   shipped tags (`v0.0.1-m0`, `v0.1.0-m1`, `v0.2.0-m2`, `v0.3.0-m3`).
6. `cmake --build build && ctest --test-dir build` — confirm green
   tree (277 passing at M4-S0).
