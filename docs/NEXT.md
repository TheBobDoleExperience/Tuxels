# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M5 active — Layer Groups.** Plan locked at
`/home/james/.claude/plans/let-s-make-a-plan-resilient-engelbart.md`.
S0 + S1 shipped:

- **S0** (commit `57b07d4`, 317 tests / 34 execs): tree refactor
  (`LayerTree` recursive helpers + `GroupLayer` skeleton),
  active-layer state migrated from flat index to `LayerId`,
  `CropCommand` snapshot keyed by id, `BlendMode::PassThrough`
  appended, `LayerKind::Group` added.
- **S1** (341 tests / 35 execs): `composeChildren` recursion + two
  group dispatchers — `composeGroupPassThrough` (snapshot+lerp,
  hot-path inlines when opacity=1/no-mask/no-clip) and
  `composeGroupIsolated` (private accum2 + scope-local
  lastBaseAlpha2 → back-composite via `compositePixel`). Clipped
  groups with no base = no-op. New `test_compose_groups.cpp` (12
  cases) covers leak/no-leak, opacity multiplies, mask gating,
  nested mixed modes, clip-on-group, clip-inside-isolated, empty
  group, visible=false, partial-leak.

**Start here: S2 — `.txl` v5 with section dividers.** Bump
`kVersionCurrent = 5` in `src/io/TxlIO.cpp`. Add
`kLayerKindOpenGroup = 10` and `kLayerKindCloseGroup = 11`. Writer
recurses depth-first: for each group, emit OpenGroup record (kind=10
+ all the existing per-layer header fields + group descriptor =
1-byte `IsExpanded` + mask tiles if hasMask), then recurse into
children, then emit CloseGroup record (kind=11, all-zero payload).
`NumLayers` semantic shifts from "tree size" to "record count on
disk" — a group contributes 2 records. Reader maintains a
`std::vector<GroupLayer*> groupStack` (empty = root): on OpenGroup,
construct + push + attach to current parent; on CloseGroup, pop
(error on underflow); existing kinds 1-5 attach to current parent.
Group masks are doc-sized (extend the existing pixel-vs-adjustment
mask-dim branch). Add ~6 round-trip cases in `test_txl_io.cpp`:
empty group, three pixel children, three-deep nesting, group
attributes (mask + clipToBelow + Multiply blend + opacity 0.7 +
isExpanded=false), mixed root ordering, v4 fixture loads with no
groups.

After S2: S3 (panel indent + chevron + New Group menu), S4
(Group/Ungroup commands + cross-parent move substrate), S5
(polish), S6 (tag).

Cold-start verification:

```
cmake --build build && ctest --test-dir build
QT_QPA_PLATFORM=offscreen timeout 2 ./build/tuxels
```

Expect 341 passing across 35 executables at S1 (M5 in progress).

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state (M5 active, S0 + S1 ✅).
2. This file — S2 entry point + remaining steps.
3. `cat /home/james/.claude/plans/let-s-make-a-plan-resilient-engelbart.md`
   — full M5 plan with per-step DoD + tests + critical files.
4. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
5. `cat /home/james/.claude/plans/quirky-napping-koala.md` — M4 plan
   archive (reference only; M4 shipped).
6. `git log --oneline -20` + `git tag --list` — recent commits and
   shipped tags (`v0.0.1-m0`, `v0.1.0-m1`, `v0.2.0-m2`, `v0.3.0-m3`,
   `v0.4.0-m4`).
7. `cmake --build build && ctest --test-dir build` — confirm green
   tree (341 passing at S1 mid-M5).
