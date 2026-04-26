# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M5 active — Layer Groups.** Plan locked at
`/home/james/.claude/plans/let-s-make-a-plan-resilient-engelbart.md`.
S0 landed 2026-04-26 (317 tests across 34 executables): tree
refactor (`LayerTree` recursive helpers + `GroupLayer` skeleton),
active-layer state migrated from flat index to `LayerId`,
`CropCommand` snapshot keyed by id, `BlendMode::PassThrough`
appended, `LayerKind::Group` added. Compose currently walks
`tree.flatten()` and skips Group nodes (S1 turns on real recursion).

**Start here: S1 — compose recursion.** Replace the S0 flatten-stub
in `src/compositor/compose.cpp` with `composeChildren` — Pass-
Through groups share their parent's accumulator + `lastBaseAlpha`
(snapshot accum, recurse, lerp back via opacity*mask); non-Pass-
Through groups allocate private `accum2` + scope-local
`lastBaseAlpha2` + `hasBase2 = false`, recurse, then composite back
via the group's blend/opacity/mask with `f *= clipToBelow ?
lastBaseAlpha[idx] : 1`. Add `test_compose_groups.cpp` with ~10
cases (Pass-Through bit-equals flat, Pass-Through+adjustment leaks
out, isolated+adjustment doesn't leak, opacity multiplies, group
mask gates, nested mixed modes, clip-on-group, clip-inside-isolated,
empty group no-op, visible=false skips). Defensive `case
PassThrough → bm_normal` already in `applyBlend` (added S0).

After S1: S2 (`.txl` v5 section dividers), S3 (panel indent +
chevron), S4 (Group/Ungroup commands), S5 (polish), S6 (tag).

Cold-start verification:

```
cmake --build build && ctest --test-dir build
QT_QPA_PLATFORM=offscreen timeout 2 ./build/tuxels
```

Expect 317 passing across 34 executables at S0 (M5 in progress).

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state (M5 active, S0 ✅).
2. This file — S1 entry point + remaining steps.
3. `cat /home/james/.claude/plans/let-s-make-a-plan-resilient-engelbart.md`
   — full M5 plan with per-step DoD + tests + critical files.
4. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
5. `cat /home/james/.claude/plans/quirky-napping-koala.md` — M4 plan
   archive (reference only; M4 shipped).
6. `git log --oneline -20` + `git tag --list` — recent commits and
   shipped tags (`v0.0.1-m0`, `v0.1.0-m1`, `v0.2.0-m2`, `v0.3.0-m3`,
   `v0.4.0-m4`).
7. `cmake --build build && ctest --test-dir build` — confirm green
   tree (317 passing at S0 mid-M5).
