# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M5 active — Layer Groups.** Plan locked at
`/home/james/.claude/plans/let-s-make-a-plan-resilient-engelbart.md`.
S0–S3 shipped (model + compose + IO + UI):

- **S0** (commit `57b07d4`, 317 / 34): tree refactor,
  active-id migration, CropCommand id-keyed snapshot.
- **S1** (commit `4692f95`, 341 / 35): `composeChildren`
  recursion with Pass-Through and isolated branches.
- **S2** (commit `19a4898`, 347 / 35): `.txl` v5 PSD-style
  OpenGroup/CloseGroup section dividers; v1–v4 still load.
- **S3** (353 / 36): `LayerRowWidget` chevron + folder thumb +
  unified `setIndentDepth` + Pass-Through blend combo for groups
  only; `LayersPanel::buildDisplayList` recursive top-down walk
  honoring `isExpanded`; `MainWindow::onLayerNewGroup` slot +
  `Layer → New Group` menu; disabled placeholders for `Group
  Layer` (Ctrl+G) and `Ungroup Layer` (Ctrl+Shift+G). 6 new
  panel-group test cases.

**Start here: S4 — Group/Ungroup commands + cross-parent
`MoveLayerCommand` substrate.** Files:
`src/history/MoveLayerCommand.{h,cpp}` — extend with cross-parent
ctor `(Document*, LayerId, LayerId beforeParentId, int beforeIdx,
LayerId afterParentId, int afterIdx)`; existing same-parent ctor
delegates with equal parent ids; `undo()` and `redo()` use
`tree.move(parentLookup(...), idx, parentLookup(...), idx)`;
substrate ready for M6 drag-reorder.
`src/app/MainWindow.{h,cpp}`:
- `onLayerGroupActive` (Ctrl+G): capture active id + `tree.locate`
  parent + index. doIt: pluck active layer from parent via
  `removeFromPath`, install new GroupLayer at same parent + index
  via `insertAtPath`, push the layer into group's `children` at
  index 0, set active = group's id. undoIt: extract layer back
  from group, remove group, reinstall layer at original parent +
  index. Stash both `unique_ptr<LayerBase>` instances in a
  `shared_ptr<...>` for redo identity. Group default name "Group
  N", PassThrough blend, opacity 1, no mask.
- `onLayerUngroupActive` (Ctrl+Shift+G): capture group + parent +
  index. doIt: stash group's children, remove group from parent,
  re-insert children into parent at the group's old index; set
  active = bottom-most ex-child id. undoIt: rebuild the group
  (same instance from stash) with stashed children, install at
  captured parent + index, restore active.
- Refine `onLayerNewGroup`: insert at active layer's parent +
  (active's index + 1) — visually above the active layer in the
  panel. When no active, insert at root.
- Menu enable/disable wired on `aboutToShow` (Group Layer enabled
  iff active layer; Ungroup Layer enabled iff active layer is a
  Group).
`src/ui/LayersPanel.cpp` Up/Down handlers — scope-local moves
only (children of the same parent); use `tree.locate(activeId)`
for parent + idx.
New `tests/test_group_commands.cpp` with ~8 cases (group active
wraps; undo restores; ungroup promotes children; undo rebuilds;
ungroup empty just deletes; group→ungroup→undo→undo loop;
Up/Down inside a group is scope-local; New Group inside a group
inserts as sibling at active+1).

After S4: S5 (polish), S6 (verify + tag).

Cold-start verification:

```
cmake --build build && ctest --test-dir build
QT_QPA_PLATFORM=offscreen timeout 2 ./build/tuxels
```

Expect 353 passing across 36 executables at S3 (M5 in progress).

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state (M5 active, S0–S3 ✅).
2. This file — S4 entry point + remaining steps.
3. `cat /home/james/.claude/plans/let-s-make-a-plan-resilient-engelbart.md`
   — full M5 plan with per-step DoD + tests + critical files.
4. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
5. `cat /home/james/.claude/plans/quirky-napping-koala.md` — M4 plan
   archive (reference only; M4 shipped).
6. `git log --oneline -20` + `git tag --list` — recent commits and
   shipped tags (`v0.0.1-m0`, `v0.1.0-m1`, `v0.2.0-m2`, `v0.3.0-m3`,
   `v0.4.0-m4`).
7. `cmake --build build && ctest --test-dir build` — confirm green
   tree (353 passing at S3 mid-M5).
