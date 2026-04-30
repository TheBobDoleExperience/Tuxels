# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M6 shipped** as `v0.6.0-m6` on 2026-04-30 (387 cases / 40 execs):
- M6-S0 PropertiesPaneGroup + dock binding (`1e337e3`)
- M6-S1 LayersPanel D&D + cross-parent move (`c0bddc8`)
- M6-S2 multi-select + batch delete/group/visibility (`fa5c047`)
- M6-S3 docs + tag

Tag pushed; main pushed.

**Start here: M7 kickoff discussion.** No plan committed yet. M5
left several follow-ups; M6 picked the smallest three of the six
(group props, D&D, multi-select). The remaining three plus a few
new candidates that surfaced during M6 are listed below — pick a
direction with the user before drafting a plan.

## M7 Candidates

1. **Tools acting on multi-select.** Move on bbox-union (drag the
   union; per-layer origin shifts share the delta), Free Transform
   on bbox-union (one transform applied to N layers), Transform on
   bbox-union, Paint stays single-active. Substrate is in place
   (`Document::selectedLayerIds_`); per-tool semantics need design
   + implementation. Mid-large scope; PS users expect this once
   they've tasted multi-select.

2. **PSD import (read-only).** SCOPE.md §5.1 — deferred since M0.
   The .txl v5 section-divider scheme matches PSD's flat-with-
   markers structure, so the parse-stack pattern transfers. Adjust
   ment kinds (Levels / Curves / HS / BC) round-trip through .txl
   v3 already, so M7 PSD reader can map to existing types. Largest
   pure-format work; binary edge cases are gnarly. Scope-bound to
   READ ONLY for M7 — write is a separate milestone.

3. **Smart Objects.** Re-editable embedded sub-documents. Recursive
   compose is already done. .txl extension + separate undo stack
   per child window are the new pieces. Whole-milestone scope.

4. **Polish round.** Tablet pressure for brush dynamics (deferred
   since M2); ToolsPanel section-state persistence (`QSettings`);
   "Up/Down crossing group boundaries" via menu / Alt+Up; group
   color labels; "Isolate adjustments" toggle separate from blend
   mode; visual drop indicator while dragging in LayersPanel
   (today the system Qt cursor is the only feedback). Each item
   <1 hr; bundle 3-5 into a single milestone.

5. **Performance pass.** Multi-thread compose per tile; per-tile
   GPU upload / display; lazy histogram recompute; mask paint hot
   path optimization. Largest engineering scope, no user-visible
   features. Schedule when the editor starts feeling sluggish on
   real-world docs.

6. **Drag the LayersPanel drop indicator.** A visual line/bar
   showing where a drop will land (Above / On / Below) — today the
   user has to predict from cursor position. Small follow-up to
   M6-S1 (~1 hr).

Kickoff agenda: pick the scope, draft a plan, commit via
ExitPlanMode. Ask the user which direction they want.

Cold-start verification:

```
cmake --build build && ctest --test-dir build
QT_QPA_PLATFORM=offscreen timeout 2 ./build/tuxels
```

Expect 40 passing executables (387 internal cases) at `v0.6.0-m6`.

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state (M6 ✅ shipped, M7 TBD).
2. This file — M7 kickoff candidates.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions
   (M5-S0 added recursive `LayerTree`; M5-S1 added Pass-Through
   vs. isolated compose recursion; M5-S2 added `.txl` v5 section
   dividers; M6-S0 added GroupProperties / PropertiesPaneGroup;
   M6-S1 added LayersPanel custom DnD pipeline + cycle detection;
   M6-S2 added Document::selectedLayerIds_ multi-selection set).
4. `cat /home/james/.claude/plans/lets-take-a-look-iterative-platypus.md`
   — M6 plan archive (reference only; M6 shipped).
5. `cat /home/james/.claude/plans/let-s-make-a-plan-resilient-engelbart.md`
   — M5 plan archive (reference only).
6. `git log --oneline -20` + `git tag --list` — recent commits and
   shipped tags (`v0.0.1-m0` through `v0.6.0-m6`).
7. `cmake --build build && ctest --test-dir build` — confirm
   green tree (40 executables / 387 cases at `v0.6.0-m6`).
