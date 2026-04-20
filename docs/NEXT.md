# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M2 shipped as `v0.2.0-m2`** (tagged + pushed 2026-04-21). Everything in
the plan landed, the user sign-off walkthrough passed, and one
regression found during that walkthrough is fixed with a dedicated
integration test (commit `4492355`: Transform Enter path now calls
`cmd->apply()` before `undoStack_->push(...)` — `UndoStack::push` expects
the side-effect to already be applied, and Transform's preview is an
overlay so the real layer was untouched during the drag).

Next: **M3 kickoff discussion.**

## M3 kickoff discussion

Adjustments (Levels / Curves) were deferred from the M2 kickoff because
they don't share a natural scope boundary with the manipulation tools.
They're the working headliner for **M3 — Adjustments & Non-destructive
Edits**, but before writing a plan, reach alignment with the user on:

- **Scope boundary:** Levels + Curves first, or a wider "Image → Adjust"
  family (Hue/Saturation, Brightness/Contrast, Color Balance, Threshold)?
- **Non-destructive route:** do adjustments land as full **adjustment
  layers** in M3 (Photoshop parity; requires clip-to-layer semantics and
  a `LUT` / `Curve` serialization path through `.txl`), or as modal
  **destructive commits** first with adjustment layers deferred to M4?
- **Histogram UI:** in-dialog preview + live histogram is Photoshop's
  bar for Levels/Curves; needs a tile-aware histogram scan. Is this a
  blocker or a "nice to have"?
- **Related manipulation polish likely belonging here:** Shift
  aspect-lock + 15° rotation snap in Free Transform (deferred from S3),
  movable transform pivot (deferred), tablet pressure wiring for the
  already-existing `RoundBrushParams::pressureMultiplier`.

Once the user picks a scope, drop into plan mode and write the
authoritative plan at `/home/james/.claude/plans/<name>.md` the same way
M0/M1/M2 were scoped.

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state (M2 shipped as `v0.2.0-m2`).
2. This file — what to do next (M3 kickoff).
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
4. `cat /home/james/.claude/plans/cryptic-stargazing-moonbeam.md` — M2
   plan archive (S0–S7 done).
5. `git log --oneline -20` + `git tag --list` — recent commits and the
   `v0.2.0-m2` tag.
6. `cmake --build build && ctest --test-dir build` — confirm green tree
   (215 passing cases across 20 executables).
7. Pick up the M3 kickoff discussion above. No plan file yet — the user
   drives the scope conversation first, then drop into plan mode.
