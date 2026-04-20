# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M2-S5 — Select by Color.** S0/S1/S2/S3/S4 landed 2026-04-20 (layer
origins + Place + Move + Free Transform + Lasso/PolyLasso). Next up: a
non-contiguous wand that picks every pixel within a color tolerance,
ignoring connectivity.

- `src/tools/ToolId.h`: add `SelectByColor`.
- `src/tools/SelectByColorTool.{h,cpp}` (new): on click, sample the
  active PixelLayer's pixel color, walk every tile in the layer's
  backing image (skip empty tile slots), mark every pixel within an
  L∞ tolerance of the seed. Reuse `channelDist` from
  `src/fill/FloodFill.cpp:13-18`. Commit via `SelectionCommand` with
  the shared wand combine-mode plumbing (shares the wand's options
  row).
- `src/ui/ToolsPanel.{h,cpp}`: picker button; consider Shift-W cycles
  Magic Wand ↔ Select By Color (Photoshop-like grouping). Shares the
  wand options row.
- `src/app/MainWindow.cpp`: wire shortcut + tool routing + commit pop
  on `onLayerPainted`.
- Tests: `test_select_by_color` — tolerance zero (exact match only),
  mid tolerance, full tolerance (all opaque pixels), interaction with
  existing selection (Add/Subtract/Intersect).

After S5:

- S6 — Brush dynamics (size / opacity jitter, spacing).
- S7 — Verify + tag `v0.2.0-m2`.

Expected deliverables per step (mirrors M1 shape):

1. Shipping UI wiring with menu + shortcut.
2. Unit tests for the headless / pure-C++ pieces (lives in
   `tuxels_core` when possible).
3. STATUS / NEXT / ARCHITECTURE doc updates.
4. A user-verification step at the end of the milestone before
   tagging `v0.2.0-m2`.

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state.
2. This file — what to do next.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
4. `cat /home/james/.claude/plans/cryptic-stargazing-moonbeam.md` — M2
   plan (authoritative).
5. `git log --oneline -10` — recent commits.
6. `cmake --build build && ctest --test-dir build` — confirm green
   tree (expect 199 passing test cases — 18 ctest executables — as of
   M2-S4 ship).
7. Pick up "Immediately Next" above.
