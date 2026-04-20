# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M1 — scope & plan.** M0 shipped as `v0.0.1-m0` on 2026-04-20. User has
asked for, in their own words:

1. "More tools added to the tool bar (cropping, bucket tool, smart
   selection, etc.)"
2. "Tuxels should have its own proprietary file type for non-destructive
   editing (I was thinking `.txl`)"
3. PSD support "can come later (or whenever you think it's best to
   incorporate it into the workflow)"

The sequencing recommendation waiting for user confirmation: do a
**Selection & Fill** milestone (M1) before the native file format, because
crop / bucket / smart-select all sit on a selection-mask data structure
that must exist first. The file format then serializes Document + layers
+ masks + the new selection primitives in one shot, which means we design
`.txl` after the model it has to persist is stable.

Proposed M1 skeleton (user to confirm / redirect):

- **S1** — Selection model: `SelectionMask` type (1-channel float image),
  `Document::selection()` accessor, compositor + brush engine respect
  selection alpha (clip paint to selection on write).
- **S2** — Rectangular marquee tool (Add / Subtract / Intersect
  modifiers), marching-ants overlay in CanvasView (timer-driven dashed
  outline of selection edge).
- **S3** — Bucket fill tool (scanline flood fill + tolerance slider;
  respects active selection; fills with foreground color).
- **S4** — Magic wand (color-range flood select with tolerance; builds
  selection, same tool plumbing as bucket).
- **S5** — Crop tool (marquee-based canvas resize; undoable by storing
  old dimensions + tile map).
- **S6** — `.txl` native format: writer + reader. Contains doc
  dimensions, per-layer kind/name/visibility/opacity/blend/mask, tile
  bitmap per layer (stored sparse, matching TuxImage's internal layout),
  current selection, future-compat version field. ZIP-of-binaries or
  custom chunked binary — decide at S6 start.
- **S7** — Verify + tag `v0.1.0-m1`. DoD walkthrough.

M2 candidate: **PSD I/O** (start read-only; full PSD write later).

## When User Confirms M1 Plan

1. Write milestone plan into `/home/james/.claude/plans/` (pick a fresh
   codename).
2. Create tasks #1..#7 in the task list, one per step.
3. Start S1 (selection model). Keep 62-test bar green; add tests per
   step.

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state.
2. This file — what to do next.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
4. `git log --oneline -10` — recent commits.
5. `cmake --build build && ctest --test-dir build` — confirm green tree
   (expect 62 passing tests as of v0.0.1-m0).
6. Pick up "Immediately Next" above.
