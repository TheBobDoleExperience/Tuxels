# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M10 shipped** as `v0.10.0-m10` on 2026-04-30 (407 cases / 41 execs):
- M10-S0 compose() span of overrides (`25dd3d0`)
- M10-S1+S2 TransformTool multi-source + batched commit (`fad8eff`)
- M10-S3 docs + tag

Tag pushed; main pushed. The M7-deferred Free-Transform-multi-select
work is finally landed.

**Start here: M11 kickoff.** With the headline architectural item shipped,
the deferred list is back to feature additions and polish.

## M11 Candidates

1. **PSD import (read-only).** SCOPE.md §5.1 — deferred since M0.
   Largest pure-format work; binary edge cases gnarly. The .txl v5
   section-divider scheme matches PSD's flat-with-markers structure;
   adjustment kinds round-trip through .txl v3 already. Map PSD blend
   ordinals to the local enum, RLE-decompress channel data, walk the
   layer-and-mask-info section's flat list. Read-only for M11; write is
   a separate milestone.

2. **Smart Objects.** Re-editable embedded sub-documents (a `Document`
   inside a layer). Recursive compose already done (M5-S1). New pieces:
   .txl extension for embedded docs; separate undo stack per child
   window; smart-object pixel render on demand.

3. **Layer effects (drop shadow, glow, stroke, inner shadow, …).** PS
   uses these heavily. Big surface area — each effect is its own pixel
   shader / filter; the layer's render path needs a new
   `applyEffectsToOutput()` hook between renderTile and blend.

4. **Text layers.** Big. New `TextLayer` kind; Qt's `QPainter` +
   `QFontMetrics` for glyph layout; resterize on demand into a tile-
   sparse TuxImage.

5. **Brush mid-stroke seamless cursor radius update.** Today the cursor
   ring is fixed to `brush.diameter`; with M8-S1 pressure, the
   effective diameter shrinks dynamically and the ring no longer
   matches the actual stamp footprint. Small (~30 min).

6. **Performance pass.** Multi-thread `composeTileRange` per tile;
   per-tile GPU upload via QOpenGLTexture; lazy histogram recompute.
   Largest engineering scope, no user-visible features.

7. **"Isolate adjustments" toggle** for groups (separate from blend
   mode). One bool on GroupLayer + .txl v7.

8. **Merge Down through groups / adjustments.** M9-S0 only handles
   pixel-into-pixel.

9. **Color picker tool (Eyedropper).** PS keyboard `I`. Reads pixel
   under cursor, sets foreground color. Optional Alt+click while in
   the Brush tool.

10. **Layer search / filter** in LayersPanel. Type-to-filter the row
    list — useful in deeply nested docs.

Kickoff agenda: pick the scope, draft a plan, commit via
ExitPlanMode.

Cold-start verification:

```
cmake --build build && ctest --test-dir build
QT_QPA_PLATFORM=offscreen timeout 2 ./build/tuxels
```

Expect 41 executables / 407 internal cases at `v0.10.0-m10`.

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state (M10 ✅ shipped, M11 TBD).
2. This file — M11 kickoff candidates.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions:
   - M5 layer tree + recursive compose + .txl v5 sections
   - M6 group properties / D&D / multi-select
   - M7 polish (multi-select Move, cross-scope Up/Down, drop indicator,
     persistence, Layer Duplicate, rename, context menu)
   - M8 color labels (.txl v6) / tablet pressure / Rasterize
   - M9 Merge Down
   - M10 compose span overrides + TransformTool multi-source
4. `git log --oneline -25` + `git tag --list` — recent commits and
   shipped tags (`v0.0.1-m0` through `v0.10.0-m10`).
5. `cmake --build build && ctest --test-dir build` — confirm
   green tree (41 executables / 407 cases at `v0.10.0-m10`).
