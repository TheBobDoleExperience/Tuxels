# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M5 active — Layer Groups.** Plan locked at
`/home/james/.claude/plans/let-s-make-a-plan-resilient-engelbart.md`.
S0–S5 shipped:

- **S0** (commit `57b07d4`, 317 / 34): tree refactor + active-id
  migration + CropCommand id-keyed snapshot.
- **S1** (commit `4692f95`, 341 / 35): `composeChildren` recursion
  with Pass-Through and isolated branches.
- **S2** (commit `19a4898`, 347 / 35): `.txl` v5 PSD-style
  OpenGroup/CloseGroup section dividers; v1–v4 still load.
- **S3** (commit `3929f2c`, 353 / 36): chevron + folder thumb +
  indent + Pass-Through combo + depth-aware panel walk + `Layer →
  New Group` menu.
- **S4** (commit `d20a801`, 362 / 37): Group / Ungroup commands
  (Ctrl+G, Ctrl+Shift+G); New Group inserts at active+1; Up/Down
  scope-local; menu enable/disable per active state.
- **S5** (364 / 37): `onLayerDelete` group-aware (uses
  `removeFromPath`); PropertiesDock empty-state on group;
  `addAdjustment*` slots route into active group / above active
  via `computeAdjustmentInsertSlot`; **bug-fix in
  `histogramBelow`** — keeps target's ancestor groups visible so
  the preview composite includes the group's earlier children. 2
  new test cases.

**Start here: S6 — verify + tag `v0.5.0-m5`.** All M5 substantive
work is done. The remaining tasks are:
1. Manual DoD walkthrough on a real multi-layer doc — open a v4
   `.txl` from M4 (loads flat); Ctrl+G to wrap a layer in a
   group; switch group blend to Multiply (isolation); add a
   Levels adjustment (lands inside the group); add a doc-sized
   group mask, paint half-coverage; Ctrl+Shift+G to ungroup;
   save → reopen (v5 round-trip with nesting + masks +
   `isExpanded`); undo-redo loop; Free Transform on inner layer;
   Crop the doc.
2. `cmake --build build && ctest --test-dir build` — expect 364
   passing across 37 executables.
3. Tag `v0.5.0-m5` and update:
   - `docs/STATUS.md` — flip M5 row to "shipped 2026-04-26 as
     `v0.5.0-m5`" with final test count.
   - `docs/NEXT.md` — point at M6 with: multi-select in
     LayersPanel, drag-and-drop reorder + drop-into-group, group
     properties pane in PropertiesDock, Up/Down crossing group
     boundaries, PSD round-trip (the section-divider scheme
     matches PSD; actual reader/writer is its own milestone),
     Smart Objects (held over from M3 deferred list), brush
     dynamics tablet pressure (deferred since M2).
   - `docs/LOG.md` — add the M5 chronicle.
   - `docs/ARCHITECTURE.md` — recursive LayerTree, Pass-Through
     vs. isolated compose semantics, `.txl` v5 section dividers.

Cold-start verification:

```
cmake --build build && ctest --test-dir build
QT_QPA_PLATFORM=offscreen timeout 2 ./build/tuxels
```

Expect 364 passing across 37 executables at S5 (M5 in progress).

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state (M5 active, S0–S5 ✅).
2. This file — S6 (verify + tag) is all that's left.
3. `cat /home/james/.claude/plans/let-s-make-a-plan-resilient-engelbart.md`
   — full M5 plan with per-step DoD + tests + critical files.
4. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
5. `cat /home/james/.claude/plans/quirky-napping-koala.md` — M4 plan
   archive (reference only; M4 shipped).
6. `git log --oneline -20` + `git tag --list` — recent commits and
   shipped tags (`v0.0.1-m0`, `v0.1.0-m1`, `v0.2.0-m2`, `v0.3.0-m3`,
   `v0.4.0-m4`).
7. `cmake --build build && ctest --test-dir build` — confirm green
   tree (364 passing at S5 mid-M5).
