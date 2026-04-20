# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M2 kickoff — planning.** M1 shipped 2026-04-20 as tag `v0.1.0-m1`
(130 tests green). See SCOPE.md for the long-range target mix; the
working shortlist for M2 (confirm with the user at kickoff):

- Brush dynamics (size/opacity jitter, pressure hooks, spacing curves)
- Additional selection tools (lasso, polygonal lasso, by-color)
- Move tool + layer transform (translate / scale / rotate preview
  with commit)
- Color adjustments pass #1 (levels or curves, driven by an
  adjustment-layer scaffold — even if the first adjustment is baked)
- Tool cursor polish + small UX follow-ups if the user surfaces more

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
4. `git log --oneline -10` — recent commits.
5. `cmake --build build && ctest --test-dir build` — confirm green
   tree (expect 130 passing tests as of M1 ship).
6. Pick up "Immediately Next" above.
