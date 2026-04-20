# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M2-S7 — Verify + tag `v0.2.0-m2`.** S0–S6 all landed 2026-04-20. What's
left is a user-facing walkthrough on the live binary + the final git tag.

The DoD walkthrough (plan §S7):

1. Open a fresh doc (or load an existing `.txl`), then `File → Place…`
   (Ctrl+Shift+P) an oversized PNG — verify it centers in the doc and
   keeps its offscreen pixels (they become visible after a Move).
2. Switch to Move (V), drag the placed layer. Confirm the live repaint
   stays snappy (partial-recomposite only the union of old+new doc
   footprint). Undo/redo should swap origins cleanly.
3. Switch to Free Transform (Ctrl+T). Drag corners to scale, inside the
   quad to translate, outside to rotate. Confirm preview is bilinear /
   premultiplied (no rotation halos). Enter commits, Escape cancels,
   tool-switch auto-commits. Undo restores the pre-transform image +
   origin.
4. Lasso (L): freehand drag to make a feathered-boundary selection.
   Polygonal Lasso (P): click vertices, close on near-start click or
   Enter. Verify Shift / Alt / Shift+Alt combine modes at press-time
   win over the persistent options row, and that the options row is the
   hijack-proof fallback on GNOME (Alt-drag goes to the WM).
5. Magic Wand (W) + Select By Color (Shift+W cycles). Click on a color
   region, verify contiguous vs non-contiguous behavior differs, the
   tolerance slider is shared (same 0–255 slider drives both), and the
   combine-mode buttons apply to both.
6. Brush (B): paint with Size Jitter / Opacity Jitter / Spacing sliders
   at non-zero values. Confirm strokes look organic (per-stamp variance)
   and that dropping jitter to 0 restores mechanical Photoshop-like
   output.
7. `File → Save As…` as `.txl`, close the doc, re-open — confirm layer
   origins, selection, active layer, and paint target all round-trip.
8. Tag: `git tag -a v0.2.0-m2 -m "M2 — Position, Shape, Stroke Quality"`
   once the user signs off.

Final test gate: `cmake --build build && ctest --test-dir build` green
(20 executables, 214 cases); no new warnings; `QT_QPA_PLATFORM=offscreen
timeout 2 ./build/tuxels` clean boot.

After S7: milestone M2 shipped. Next milestone kickoff discussion
(Adjustments — Levels/Curves — was deferred to M3 at the M2 kickoff).

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state.
2. This file — what to do next.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
4. `cat /home/james/.claude/plans/cryptic-stargazing-moonbeam.md` — M2
   plan (authoritative).
5. `git log --oneline -10` — recent commits.
6. `cmake --build build && ctest --test-dir build` — confirm green
   tree (expect 214 passing test cases — 20 ctest executables — as of
   M2-S6 ship).
7. Pick up "Immediately Next" above.
