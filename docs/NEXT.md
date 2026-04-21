# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M3 shipped** as `v0.3.0-m3` on 2026-04-22 (commit `e11aff9`, 275
tests). Adjustment-layer infrastructure + Levels + Curves +
Brightness/Contrast + Hue/Saturation are all live; `.txl` v3
round-trips all four kinds with masks; Free-Transform polish trio
(Shift aspect-lock, 15° rotation snap, movable pivot) landed in S5.

**Start here: M4 kickoff discussion.** No plan committed yet. The
three candidates deferred out of M3 or floated in scope-out lists:

1. **Adjustment-layer clip-to-layer** — explicit "this adjustment
   affects only the layer immediately below" flag. PS calls it
   "Create Clipping Mask". Small scope: compose-time branch that
   narrows the kind==Adjustment accumulator to just one sibling
   instead of the full composite below. Touches `compose()`,
   `LayerRowWidget` (indent affordance + right-arrow glyph), the
   adjustment-layer body (one bool + serialization), `.txl` (no
   version bump — packs into the existing kind-specific
   descriptor or a flag byte).

2. **Properties dock** — dockable non-modal panel that replaces
   M3's modal dialogs. Click an adjustment → its params load into
   the dock; edit live; no OK/Cancel, live commits via
   `LayerParamsCommand` on drag-end (not on every slider tick).
   Larger scope: Qt docking, tab-per-adjustment-type, maintaining
   the "snapshot on select / commit on release" undo discipline
   without the modal reject() escape hatch.

3. **Smart objects** — re-editable embedded sub-documents. A
   smart-object layer holds a `std::unique_ptr<Document>` + a
   cached rasterization at the parent's resolution; edits open a
   child window; save propagates up. Largest scope — new layer
   kind, recursive compose, `.txl` format extension (nested
   document chunks), separate undo stack per child. Strong PSD-
   parity value but probably a whole-milestone effort on its own.

Kickoff agenda: pick the scope (one, two, or all three), draft a
Plan, commit to it via ExitPlanMode. Ask the user which direction
they want.

Cold-start verification:

```
cmake --build build && ctest --test-dir build
QT_QPA_PLATFORM=offscreen timeout 2 ./build/tuxels
```

Expect 275 passing across 27 executables at `v0.3.0-m3`.

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state (M3 ✅ shipped, M4 TBD).
2. This file — M4 kickoff candidates + kickoff agenda.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
4. `cat /home/james/.claude/plans/floofy-spinning-sedgewick.md` — M3
   plan archive (reference only; M3 shipped).
5. `cat /home/james/.claude/plans/cryptic-stargazing-moonbeam.md` — M2
   plan archive (reference only).
6. `git log --oneline -20` + `git tag --list` — recent commits and
   shipped tags (`v0.0.1-m0`, `v0.1.0-m1`, `v0.2.0-m2`, `v0.3.0-m3`).
7. `cmake --build build && ctest --test-dir build` — confirm green
   tree (275 passing at `v0.3.0-m3`).
