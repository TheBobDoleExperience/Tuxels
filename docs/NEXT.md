# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**Tag `v0.2.0-m2`.** S0–S6 shipped 2026-04-20; S7 user walkthrough passed
2026-04-21 with one regression caught and fixed (commit `4492355`): the
Free Transform Enter path was pushing a `TransformCommand` onto the undo
stack without first applying it. Because Transform's preview is an
overlay (`LayerOverride`), the real layer was untouched — so committing
popped the overlay and compose rendered the original pixels.
`UndoStack::push`'s contract is explicit: "Push a command whose side-
effect has already been applied." The fix calls `cmd->apply()` before
`undoStack_->push(...)` and added `transform_pending_commit_writes_through_apply`
as a regression test walking the full `tool.commit() → Command →
apply() → assert state` path.

Tag the milestone when the user gives the green light:

```
git tag -a v0.2.0-m2 -m "M2 — Position, Shape, Stroke Quality"
git push origin v0.2.0-m2
```

Then move to M3 kickoff.

## After the tag — M3 kickoff discussion

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

1. `cat docs/STATUS.md` — current state (M2 user-verified, tag pending).
2. This file — what to do next.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
4. `cat /home/james/.claude/plans/cryptic-stargazing-moonbeam.md` — M2
   plan (archive — S0–S6 done, S7 user-verified).
5. `git log --oneline -20` — recent commits (look for `4492355` transform
   apply-before-push fix).
6. `cmake --build build && ctest --test-dir build` — confirm green tree
   (215 passing cases across 20 executables as of the S7 regression fix).
7. If user wants the tag: `git tag -a v0.2.0-m2 -m "..."` + push.
   Otherwise pick up "After the tag — M3 kickoff discussion" above.
