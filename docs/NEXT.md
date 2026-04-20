# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M2-S1 — Place image (`File → Place…`).** S0 landed 2026-04-20 (layer
origins + origin-aware renderTile + v2 `.txl`). Next step wires the
menu: `File → Place…` (Ctrl+Shift+P), load PNG via `loadPng`, add via
`Document::addPixelLayer(std::move(img), ox, oy, name)` centered in the
doc, wrapped in the existing `AddLayerCommand` pattern. Oversized PNGs
keep offscreen pixels for free courtesy of the S0 origin plumbing.
Ship a `test_place_image` covering load → origin → undo.

After S1:

- S2 — Move tool (V); `MoveLayerCommand` swaps origin.
- S3 — Free Transform (Ctrl+T); `geom/Resample` bilinear + bbox handles.
- S4 — Lasso + Polygonal Lasso; `SelectionMask::fillPolygon` scanline.
- S5 — Select by Color; Shift-W cycles from Wand.
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
   tree (expect 141 passing tests as of M2-S0 ship).
7. Pick up "Immediately Next" above.
