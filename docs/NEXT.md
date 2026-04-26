# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M5 shipped** as `v0.5.0-m5` on 2026-04-26 (364 tests across 37
executables) — tag created locally, **pending user DoD walkthrough**
before push:

1. Open a v4 `.txl` from M4 — loads flat (no groups). Verify.
2. Pick a layer, `Ctrl+G` → wraps in a group; panel shows group +
   indented child + chevron.
3. With the group active, switch its blend combo to Multiply.
   Composite isolates and applies Multiply over underlying layers.
4. Switch back to Pass-Through. Add a Levels adjustment via
   `Layer → New Adjustment Layer → Levels…` — lands inside the
   group at the top of its children. Levels affects the entire
   composite below the group (leaks out, since group is PT).
5. Switch group blend to Normal. Levels now affects only the
   group's contents (group is isolated).
6. Add a doc-sized mask to the group via `Layer → Add Layer Mask`.
   Paint half-coverage. Group output is gated.
7. `Ctrl+Shift+G` → Ungroup. Children promoted; Levels now affects
   everything below it again.
8. Save → reopen → all groups round-trip with nesting + masks +
   blend + expansion state (`isExpanded`).
9. Undo-redo loop steps 7→6→5→4→3→2 and back. Tree shape matches
   at each rest stop.
10. Free Transform (Ctrl+T) on a layer inside a group — works.
11. Crop the doc → group masks + child images all crop correctly;
    undo restores.
12. `cmake --build build && ctest --test-dir build` — expect 364
    across 37.

If verification finds an issue: `git tag -d v0.5.0-m5`, fix in a
new commit, re-tag. Otherwise: `git push origin main v0.5.0-m5`.

**Start here: M6 kickoff discussion.** No plan committed yet. M5
shipped layer groups end-to-end but explicitly punted several
follow-ups; M6 candidates:

1. **Multi-select in LayersPanel.** Drag-shift-click range +
   Ctrl-click toggle + anchor-row tracking through reorder. Each
   tool has to decide what multi-active means (Move yes; Paint
   no; Transform yes-with-bbox-union; Free Transform yes). Plus
   command-shaping: batch delete, batch visibility, batch group.
   Largest M6 candidate; PS users will reach for this first.

2. **Drag-and-drop layer reorder + drop-into-group.** Cross-group
   migration via the panel — currently only available via
   Group/Ungroup commands. Substrate work for `MoveLayerCommand`
   to take cross-parent params (was deferred from M5-S4 plan).

3. **Group properties pane in PropertiesDock.** Group name +
   blend + opacity + clip + (eventually) "isolate adjustments"
   toggle + named-group color labels. Hook is ready
   (`bindActiveAdjustmentToDock`'s explicit Group arm calls
   `bindNothing()` today).

4. **PSD import.** SCOPE.md §5.1 has been deferred since M0; M5's
   `.txl` v5 section-divider scheme matches PSD's flat-with-
   markers structure, so the parse-stack pattern transfers.
   Largest pure-format work; the binary is gnarly with endless
   edge cases.

5. **Smart Objects.** Re-editable embedded sub-documents (a
   `Document` inside a layer). Held over from M3's deferred list
   and M5's stress-test discussion. Whole-milestone scope —
   recursive compose (already done!), `.txl` extension, separate
   undo stack per child window.

6. **Polish round.** Tablet pressure for brush dynamics (deferred
   since M2); ToolsPanel section-state persistence (`QSettings`);
   "Up/Down crossing group boundaries" (M5 kept it scope-local —
   M6 either adds explicit menu items or covers it via #2).

Kickoff agenda: pick the scope, draft a plan, commit via
ExitPlanMode. Ask the user which direction they want.

Cold-start verification:

```
cmake --build build && ctest --test-dir build
QT_QPA_PLATFORM=offscreen timeout 2 ./build/tuxels
```

Expect 364 passing across 37 executables at `v0.5.0-m5`.

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state (M5 ✅ shipped, M6 TBD).
2. This file — M6 kickoff candidates + walkthrough checklist.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions
   (M5-S0 added recursive `LayerTree`; M5-S1 added Pass-Through
   vs. isolated compose recursion; M5-S2 added `.txl` v5
   section dividers).
4. `cat /home/james/.claude/plans/let-s-make-a-plan-resilient-engelbart.md`
   — M5 plan archive (reference only; M5 shipped).
5. `cat /home/james/.claude/plans/quirky-napping-koala.md` — M4
   plan archive (reference only).
6. `git log --oneline -20` + `git tag --list` — recent commits
   and shipped tags (`v0.0.1-m0`, `v0.1.0-m1`, `v0.2.0-m2`,
   `v0.3.0-m3`, `v0.4.0-m4`, `v0.5.0-m5`).
7. `cmake --build build && ctest --test-dir build` — confirm
   green tree (364 passing at `v0.5.0-m5`).
