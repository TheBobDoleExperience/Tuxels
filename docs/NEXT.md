# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M2-S6 — Brush dynamics.** S0/S1/S2/S3/S4/S5 landed 2026-04-20 (layer
origins + Place + Move + Free Transform + Lasso/PolyLasso + Select By
Color). Next up: per-stamp size/opacity jitter so the brush feels less
mechanical, plus a Spacing slider surfaced alongside.

- `src/brush/RoundBrush.h`: extend `RoundBrushParams` with `float sizeJitter = 0.f, opacityJitter = 0.f, pressureMultiplier = 1.f;`
  (spacingRatio already exists — expose it in the UI, below).
- `src/brush/BrushEngine.{h,cpp}` (`applyStamp`, `beginStroke`,
  `continueStroke`): thread-local `std::mt19937` seeded from a hash of
  a per-stroke id (monotonic counter bumped in `beginStroke`). Each
  stamp draws `U(-jitter,+jitter)` → `diameter *= 1 + u`, clamped to
  `[1, 2×diameter]`; same for opacity but clamped to `[0, 1]`. With
  `sizeJitter = 0` + `opacityJitter = 0`, output must be bitwise
  identical to the current brush (regression floor).
- `pressureMultiplier` is an input stub — tablet integration deferred
  to M3+. Leave it on the params and thread into `applyStamp` so later
  tablet work slots in without touching the params struct.
- `src/ui/ToolsPanel.{h,cpp}`: Brush options row gains **Size Jitter**,
  **Opacity Jitter**, **Spacing** sliders (0–100 % each, where spacing
  maps to `spacingRatio` ∈ `[0.05, 1.0]`). Slider changes push into the
  brush via the existing setter pattern.
- Tests: `test_brush_dynamics` — jitter=0 is bitwise identical to the
  pre-change brush on a fixed stroke; jitter>0 produces bounded,
  *deterministic* variance (same seed → same pixels, different seeds
  → different pixels); spacing slider changes visible stamp count
  on a long stroke.

After S6:

- S7 — Verify + tag `v0.2.0-m2`. User walkthrough through the full M2
  feature set; round-trip through `.txl`; tag.

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
   tree (expect 207 passing test cases — 19 ctest executables — as of
   M2-S5 ship).
7. Pick up "Immediately Next" above.
