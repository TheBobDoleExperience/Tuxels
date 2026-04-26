# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M5 active — Layer Groups.** Plan locked at
`/home/james/.claude/plans/let-s-make-a-plan-resilient-engelbart.md`.
S0–S4 shipped:

- **S0** (commit `57b07d4`, 317 / 34): tree refactor + active-id
  migration + CropCommand id-keyed snapshot.
- **S1** (commit `4692f95`, 341 / 35): `composeChildren` recursion
  with Pass-Through and isolated branches.
- **S2** (commit `19a4898`, 347 / 35): `.txl` v5 PSD-style
  OpenGroup/CloseGroup section dividers; v1–v4 still load.
- **S3** (commit `3929f2c`, 353 / 36): chevron + folder thumb +
  indent + Pass-Through combo + depth-aware panel walk + `Layer →
  New Group` menu.
- **S4** (362 / 37): Group / Ungroup wired (Ctrl+G,
  Ctrl+Shift+G); New Group inserts at active+1; Up/Down scope-
  local with status-bar feedback at top/bottom of group; menu
  actions enable/disable per active state. 9 new
  test_group_commands cases.

**Start here: S5 — polish.** Mostly cleanup and dock-empty-state
plumbing for groups. Files:
- `src/app/MainWindow.cpp` — `onLayerDelete` currently uses
  `activeLayerIndex()` shim + `tree.removeAt(idx)` which only
  works at root. Update the slot to use `tree.locate(activeId)` +
  `removeFromPath` so deleting a layer inside a group works
  cleanly. When the active layer IS a group, the unique_ptr
  ownership chain transitively destroys the children — undo
  restores the whole subtree by pulling the stashed group back in.
- `src/ui/PropertiesDock.cpp` — `bindActiveAdjustmentToDock`
  already falls through to `bindNothing()` when the active is a
  group (no `dynamic_cast` matches), but make the Group arm
  explicit for clarity + as a hook for the future group-properties
  pane.
- `MainWindow::onLayerAdd*` (the four addAdjustment slots) —
  current `Document::addAdjustmentLayer<T>` always appends at
  root. When active is inside a group, route the new adjustment
  into that group at (active's index + 1); when active IS a
  group, route into the group itself at the top of its children.
  Mirrors PS's context-aware Add Adjustment.
- `MainWindow::histogramBelow` already walks `tree.flatten()`
  (S0 prep) — verify with a regression test for layers inside
  groups.
- New cases: `delete_active_group_with_children_round_trip`,
  `clip_to_below_on_group_gates_compositing` (extension of
  `test_compose_groups`), `histogram_below_layer_inside_group`.

After S5: S6 — DoD walkthrough on a real multi-layer doc + tag
`v0.5.0-m5`. Update docs/STATUS.md (M5 ✅), docs/NEXT.md (M6
candidates), docs/LOG.md, docs/ARCHITECTURE.md.

Cold-start verification:

```
cmake --build build && ctest --test-dir build
QT_QPA_PLATFORM=offscreen timeout 2 ./build/tuxels
```

Expect 362 passing across 37 executables at S4 (M5 in progress).

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state (M5 active, S0–S4 ✅).
2. This file — S5 entry point + remaining steps.
3. `cat /home/james/.claude/plans/let-s-make-a-plan-resilient-engelbart.md`
   — full M5 plan with per-step DoD + tests + critical files.
4. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
5. `cat /home/james/.claude/plans/quirky-napping-koala.md` — M4 plan
   archive (reference only; M4 shipped).
6. `git log --oneline -20` + `git tag --list` — recent commits and
   shipped tags (`v0.0.1-m0`, `v0.1.0-m1`, `v0.2.0-m2`, `v0.3.0-m3`,
   `v0.4.0-m4`).
7. `cmake --build build && ctest --test-dir build` — confirm green
   tree (362 passing at S4 mid-M5).
