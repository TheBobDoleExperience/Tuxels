# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**S10 — End-to-end manual verification + tag v0.0.1-m0**

Nine of ten M0 steps are done. One sweep through every DoD bullet remains,
then tag.

Walk through every DoD bullet in
`/home/james/.claude/plans/modular-singing-teacup.md`:

1. Launch tuxels; window appears.
2. File → Open a PNG; layer appears with its filename.
3. File → New at custom dimensions.
4. Visibility toggle, opacity slider, blend-mode combo — thumbnails refresh
   and canvas recomposites. Undo each.
5. Add / delete / move-up / move-down / activate layers via toolbar and
   menu. Undo each.
6. Paint with the brush on a layer. `[` / `]` resize; status bar reports.
   Undo the stroke.
7. Layer → Add Layer Mask on the active layer. Paint on the mask (black to
   hide, white to reveal — default brush color is black, so paint hides).
   Shift-click the mask thumb to toggle enabled — composite shows the
   difference. Right-click → Delete Mask. Undo each of those.
8. Cycle through all 13 blend modes on the green disc in the sample doc
   and spot-check that each produces a plausible composite.
9. `Ctrl+Z` / `Ctrl+Shift+Z` — undo and redo through a full session of the
   above gestures.
10. Export As PNG — open the result in an external viewer (`xdg-open
    /tmp/foo.png` or `eog`) and confirm it matches the on-screen
    composite.

Any regressions found → file under "Known Issues" in STATUS.md and either
fix or defer with clear notes.

When done and green: `git tag v0.0.1-m0`, append an "M0 complete" entry to
LOG.md, flip S10 to ✅ in STATUS.md.

## Then — Pick M1

Candidates, ordered by value to user:
- **Selection tools** (rectangular / lasso) — cornerstone, unlocks
  fill/stroke/clear within a region.
- **Text layer** — useful for annotations on exports; drags in
  FreeType/HarfBuzz.
- **Adjustment layers** (HSL, Curves, Levels) — high ROI for demos; no I/O
  dependencies.
- **PSD I/O** — slow project; start read-only.

Pick one, write a milestone plan into `/home/james/.claude/plans/`, then
start S1 of that milestone.

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state.
2. This file — what to do next.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
4. `git log --oneline -10` — recent commits.
5. `cmake --build build && ctest --test-dir build` — confirm green tree
   (expect 59 passing tests as of S9).
6. Pick up "Immediately Next" above.
