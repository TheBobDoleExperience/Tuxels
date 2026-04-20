# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M1 — S7: user verification + tag `v0.1.0-m1`.** S1–S6 all shipped
2026-04-20. 130 tests passing.

What to do in S7:

1. Run the binary (`./build/tuxels`) and exercise each M1 feature end to
   end: marquee (all four combine modes, including the persistent mode
   buttons), bucket fill, magic wand, crop, and `.txl` Save/Open round
   trip. Sanity-check that PNG export still works.
2. If the user reports issues, land fixes and bump the test count
   before tagging.
3. Once the user signs off: `git tag -a v0.1.0-m1 -m "..."` and push
   the tag. Mark S7 done in STATUS.md.
4. Start M2 planning (see SCOPE.md for the big picture; the next
   milestone target is brush dynamics / additional selection tools —
   confirm with the user at kickoff).

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state.
2. This file — what to do next.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
4. `git log --oneline -10` — recent commits.
5. `cmake --build build && ctest --test-dir build` — confirm green tree
   (expect 130 passing tests as of M1-S6).
6. Pick up "Immediately Next" above.
