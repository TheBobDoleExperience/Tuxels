# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M9 shipped** as `v0.9.0-m9` on 2026-04-30 (single-step milestone):
- M9-S0 Merge Down (`3f2b586`)
- M9-S1 docs + tag

Tag pushed; main pushed.

**Start here: M10 kickoff.** Free Transform on multi-select remains
the #1 deferred candidate — it's been pushed across M7 → M8 → M9
because it requires a deep refactor of TransformTool's single-source
architecture, not a polish step. M10 is the right place for it.

## M10 Candidates

1. **Free Transform on multi-select** (still deferred). The TransformTool's
   single-source architecture (one `src_` TuxImage, one Override, one
   PendingCommit) needs to generalise to N layers with a shared
   bbox-union as the handle bbox. Each layer gets its own scratch +
   per-layer resample; a batch Transform command holds N (image, origin)
   snapshot pairs for undo. Probably a whole milestone (M9 = "Free
   Transform multi-select"). Sketch in `src/tools/TransformTool.h`.

2. **PSD import (read-only).** SCOPE.md §5.1 — deferred since M0. The
   .txl v5 section-divider scheme matches PSD's flat-with-markers;
   adjustment kinds round-trip through .txl v3 already. Largest
   pure-format work; binary edge cases gnarly. Read-only for M9.

3. **Smart Objects.** Re-editable embedded sub-documents. Recursive
   compose already done. Whole-milestone scope.

4. **Layer effects (drop shadow, glow, stroke).** PS uses these heavily.
   Big surface area.

5. **Text layers.** Big.

6. **"Isolate adjustments" toggle** for groups (separate from blend
   mode). One bool on GroupLayer + .txl v7.

7. **Performance pass.** Multi-thread compose per tile; per-tile GPU
   upload. Largest engineering scope, no user-visible features.

8. **Brush mid-stroke seamless cursor radius update.** Today the cursor
   ring is fixed to brush.diameter; with M8-S1 pressure, the effective
   diameter shrinks dynamically and the ring no longer matches.

9. **Merge Down through groups / adjustments.** M9-S0 only handles
   Pixel-into-Pixel. Group + adjustment cases need rasterize-then-merge
   semantics; queued.

Kickoff agenda: pick the scope, draft a plan, commit via
ExitPlanMode.

Cold-start verification:

```
cmake --build build && ctest --test-dir build
QT_QPA_PLATFORM=offscreen timeout 2 ./build/tuxels
```

Expect 41 executables / 403 internal cases at `v0.9.0-m9`.

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state (M9 ✅ shipped, M10 TBD).
2. This file — M10 kickoff candidates.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions:
   - M5-S0 added recursive `LayerTree`
   - M5-S1 added Pass-Through vs. isolated compose recursion
   - M5-S2 added `.txl` v5 section dividers
   - M6-S0 GroupProperties / PropertiesPaneGroup
   - M6-S1 LayersPanel custom DnD pipeline
   - M6-S2 `Document::selectedLayerIds_` multi-selection
   - M7-S0 vector-of-PendingMove for batch origin shifts
   - M7-S1 cross-scope Up/Down via tree.move
   - M7-S2 `LayerListWidget::paintEvent` overlay + 3-zone hit-test
   - M7-S3 ToolsPanel QSettings persistence per ToolId string key
   - M7-S4 `cloneLayer` polymorphic helper + `deepCopyTuxImage`
   - M7-S5 in-place rename via QLineEdit
   - M7-S6 per-row context menu (PS-style activate-on-right-click)
   - M8-S0 LayerColorLabel + .txl v6 (`hasColorLabelByte`)
   - M8-S1 ToolBase::setPressure + BrushEngine pressure scaling
   - M8-S2 Rasterize Layer (clones children to tmp doc, composes
     isolated; `deepCopyTuxImage` now public)
   - M9-S0 Merge Down (Ctrl+E) — pixel-into-pixel only;
     same compose-isolated pattern as Rasterize but two layers
4. `git log --oneline -25` + `git tag --list` — recent commits and
   shipped tags (`v0.0.1-m0` through `v0.9.0-m9`).
5. `cmake --build build && ctest --test-dir build` — confirm
   green tree (41 executables / 403 cases at `v0.9.0-m9`).
