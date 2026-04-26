# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M4 shipped** as `v0.4.0-m4` on 2026-04-26 (commit `1d776f4`,
307 tests across 33 executables). All M3-deferred items closed:
ToolsPanel accordion, adjustment-layer clip-to-layer flag, and
non-modal Properties dock with live-preview + commit-on-release for
Levels / Curves / Hue-Sat / Brightness-Contrast.

**Start here: M5 kickoff discussion.** No plan committed yet. Three
candidates floating:

1. **Smart objects** — re-editable embedded sub-documents. A
   smart-object layer holds a `std::unique_ptr<Document>` + a cached
   rasterization at the parent's resolution; edits open a child
   window; save propagates up. Largest scope of the three — new
   layer kind, recursive compose, `.txl` format extension (nested
   document chunks), separate undo stack per child. Strong PSD-
   parity value but probably a whole-milestone effort on its own.
   This was held over from M4's scope-out list.

2. **PSD import** — round-trip Photoshop's native format. SCOPE.md
   §5.1 has been deferred since M0; it's the headline value-prop
   for Linux PS migrants and we still haven't started. Smaller than
   smart objects in pure scope (parser only, no new model concepts
   beyond what we already have for layers/masks/adjustments) but
   the binary format is gnarly and the corner cases endless.

3. **Layer groups** — folders of layers with their own visibility,
   opacity, blend mode, and (eventually) clip scope. Touches the
   compose pass (recursive layer iteration), `LayerRowWidget`
   (indent + expand/collapse — could reuse the M4 CollapsibleSection
   pattern), `.txl` v5 (nested layer records), undo (group vs child
   ops), and unlocks the smart-object hierarchy story for free.
   Mid-scope.

Plus minor polish floating around: tablet pressure for brush
dynamics (deferred since M2); ToolsPanel section-state persistence
across sessions (`QSettings`); Properties dock for non-adjustment
layer kinds (PixelLayer + smart-object params).

Kickoff agenda: pick the scope, draft a Plan, commit via
ExitPlanMode. Ask the user which direction they want.

Cold-start verification:

```
cmake --build build && ctest --test-dir build
QT_QPA_PLATFORM=offscreen timeout 2 ./build/tuxels
```

Expect 307 passing across 33 executables at `v0.4.0-m4`.

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state (M4 ✅ shipped, M5 TBD).
2. This file — M5 kickoff candidates + kickoff agenda.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
4. `cat /home/james/.claude/plans/quirky-napping-koala.md` — M4 plan
   archive (reference only; M4 shipped).
5. `cat /home/james/.claude/plans/floofy-spinning-sedgewick.md` — M3
   plan archive (reference only).
6. `git log --oneline -20` + `git tag --list` — recent commits and
   shipped tags (`v0.0.1-m0`, `v0.1.0-m1`, `v0.2.0-m2`, `v0.3.0-m3`,
   `v0.4.0-m4`).
7. `cmake --build build && ctest --test-dir build` — confirm green
   tree (307 passing at `v0.4.0-m4`).
