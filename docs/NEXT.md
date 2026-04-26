# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M5 active — Layer Groups.** Plan locked at
`/home/james/.claude/plans/let-s-make-a-plan-resilient-engelbart.md`.
S0–S2 shipped (model + compose + IO):

- **S0** (commit `57b07d4`, 317 tests / 34 execs): tree refactor,
  active-id migration, CropCommand id-keyed snapshot.
- **S1** (commit `4692f95`, 341 tests / 35 execs):
  `composeChildren` recursion with Pass-Through (snapshot+lerp,
  hot-path inlines trivial case) and isolated (private accum2 +
  back-composite via group blend) branches. New
  `test_compose_groups.cpp` (12 cases).
- **S2** (347 tests / 35 execs): `.txl` v5 with PSD-style
  section-divider records — kind 10 = OpenGroup (1-byte IsExpanded
  descriptor + doc-sized mask), kind 11 = CloseGroup (header-only
  marker). Writer recurses depth-first; reader uses a
  `groupStack` to reconstruct nesting. NumLayers shifts to
  "record count on disk" (groups contribute 2). v1–v4 still load
  with explicit version constants + gating flags. 6 new
  round-trip cases (empty group, 3-children, 3-deep nest,
  attributes, mixed-root, v4 fixture compat).

**Start here: S3 — LayersPanel: indented rows + chevron + New
Group menu.** Teach the panel to render groups as indented
chevron-headed rows with their children below at depth+1. Files:
`src/ui/LayerRowWidget.{h,cpp}` (when bound to `GroupLayer*`:
prepend chevron `▾`/`▸`, swap thumb to a folder glyph, include
"Pass Through" in the blend combo, add `setIndentDepth(int)` —
unifies the existing clipToBelow margin bump with depth-based
indent), `src/ui/LayersPanel.{h,cpp}` (replace flat-list rebuild
with depth-aware walker that visits root, emits row at current
depth, recurses into expanded groups at depth+1, skips children of
collapsed groups), `src/app/MainWindow.{h,cpp}` (`onLayerNewGroup`
slot wrapped in a `LayerOpCommand`; menu entry under `Layer → New
Group`; placeholder disabled menu items for `Group Layer`
(Ctrl+G) and `Ungroup Layer` (Ctrl+Shift+G) — wired in S4). New
`tests/test_layers_panel_groups.cpp` with ~6 cases (indented
render, chevron collapses children, active highlight on group
row, chevron signal fires, blend combo includes Pass Through for
groups only, New Group action).

After S3: S4 (Group/Ungroup commands + cross-parent move
substrate), S5 (polish), S6 (tag).

Cold-start verification:

```
cmake --build build && ctest --test-dir build
QT_QPA_PLATFORM=offscreen timeout 2 ./build/tuxels
```

Expect 347 passing across 35 executables at S2 (M5 in progress).

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state (M5 active, S0–S2 ✅).
2. This file — S3 entry point + remaining steps.
3. `cat /home/james/.claude/plans/let-s-make-a-plan-resilient-engelbart.md`
   — full M5 plan with per-step DoD + tests + critical files.
4. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
5. `cat /home/james/.claude/plans/quirky-napping-koala.md` — M4 plan
   archive (reference only; M4 shipped).
6. `git log --oneline -20` + `git tag --list` — recent commits and
   shipped tags (`v0.0.1-m0`, `v0.1.0-m1`, `v0.2.0-m2`, `v0.3.0-m3`,
   `v0.4.0-m4`).
7. `cmake --build build && ctest --test-dir build` — confirm green
   tree (347 passing at S2 mid-M5).
