# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M7 shipped** as `v0.7.0-m7` on 2026-04-30 (399 cases / 41 execs):
- M7-S0 multi-select Move (`25d4137`)
- M7-S1 Up/Down crossing groups (`0bb1650`)
- M7-S2 custom drop indicator (`74dca08`)
- M7-S3 ToolsPanel persistence (`30293be`)
- M7-S4 Layer Duplicate (Ctrl+J) (`e95bcc7`)
- M7-S5 in-place rename via double-click (`20320d5`)
- M7-S6 layer-row context menu (`7fdb460`)
- M7-S7 docs + tag

Tag pushed; main pushed.

**Start here: M8 kickoff discussion.** Free Transform on multi-select
is the headline candidate (deferred from M7-S4). Plus several smaller
items remain on the polish list.

## M8 Candidates

1. **Free Transform on multi-select** (deferred from M7-S4). Big.
   TransformTool's single-source architecture (one `src_` TuxImage,
   one `Override`, one PendingCommit) needs to generalise to N
   layers with a shared bbox-union as the handle bbox. Each layer
   gets its own scratch + per-layer resample; a batch Transform
   command holds N (image, origin) snapshot pairs for undo.
   Probably a whole milestone on its own (M8 = "Free Transform
   multi-select"). Implementation sketch in
   `src/tools/TransformTool.h:30+` for the existing pieces.

2. **PSD import (read-only).** SCOPE.md §5.1 — deferred since M0.
   The .txl v5 section-divider scheme matches PSD's flat-with-
   markers; adjustment kinds round-trip through .txl v3 already.
   Largest pure-format work; binary edge cases gnarly. Scope-bound
   to read-only for M8.

3. **Smart Objects.** Re-editable embedded sub-documents. Recursive
   compose already done. .txl extension + separate undo stack per
   child window are the new pieces. Whole-milestone scope.

4. **Tablet pressure for brush dynamics** (deferred since M2).
   `BrushEngine` has a `pressureMultiplier` stub; need to wire
   `QTabletEvent` from CanvasView through ToolBase to BrushTool.
   ~1 day if Qt's tablet API behaves; could pair with brush
   roundness/angle for full PS parity.

5. **Layer color labels.** Per-layer color tag for visual grouping.
   Renders as a thin stripe at the row's left edge. Persists via
   .txl v6 (or steal a reserved field in v5). Small (~half-day).

6. **"Isolate adjustments" toggle** for groups (separate from blend
   mode). Today the only isolation switch is choosing a non-Pass-
   Through blend; users may want isolated compose without the blend
   change. Adds one bool to GroupLayer + .txl v6.

7. **Performance pass.** Multi-thread compose per tile; per-tile
   GPU upload. Largest engineering scope, no user-visible features.

8. **Rasterize Layer.** Convert any layer kind to a flat PixelLayer
   by composing it through the current document context. Useful
   when users want to "freeze" an adjustment or group into pixels.

Kickoff agenda: pick the scope, draft a plan, commit via
ExitPlanMode.

Cold-start verification:

```
cmake --build build && ctest --test-dir build
QT_QPA_PLATFORM=offscreen timeout 2 ./build/tuxels
```

Expect 41 executables / 399 internal cases at `v0.7.0-m7`.

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state (M7 ✅ shipped, M8 TBD).
2. This file — M8 kickoff candidates.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions:
   - M5-S0 added recursive `LayerTree`
   - M5-S1 added Pass-Through vs. isolated compose recursion
   - M5-S2 added `.txl` v5 section dividers
   - M6-S0 added GroupProperties / PropertiesPaneGroup
   - M6-S1 added LayersPanel custom DnD pipeline + cycle detection
   - M6-S2 added `Document::selectedLayerIds_` multi-selection
   - M7-S0 added vector-of-PendingMove for batch origin shifts
   - M7-S1 added cross-scope Up/Down via tree.move
   - M7-S2 added `LayerListWidget::paintEvent` overlay + 3-zone hit-test factored
   - M7-S3 added ToolsPanel QSettings persistence per ToolId string key
   - M7-S4 added `cloneLayer` polymorphic helper + `deepCopyImage` for tile-COW-independent dupes
   - M7-S5 added in-place rename via QLineEdit alongside the row's label
   - M7-S6 added per-row context menu with PS-style "right-click activates"
4. `cat /home/james/.claude/plans/lets-take-a-look-iterative-platypus.md`
   — M6 plan archive (M7 ran open-ended without a plan file).
5. `git log --oneline -25` + `git tag --list` — recent commits and
   shipped tags (`v0.0.1-m0` through `v0.7.0-m7`).
6. `cmake --build build && ctest --test-dir build` — confirm
   green tree (41 executables / 399 cases at `v0.7.0-m7`).
