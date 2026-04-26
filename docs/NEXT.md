# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M4 — code-complete pending DoD.** S0 + S1 + S2 + S3 + S4 shipped
2026-04-26 across 4 commits between `e6a86e1` and HEAD.
**307 tests across 33 executables, all green.**

**S5 (verify + tag v0.4.0-m4) needs the user's hands-on walkthrough.**
Specifically:

1. **ToolsPanel accordion** — every section opens/collapses cleanly via
   its chevron; clicking a section header activates that tool (no
   collapse side-effect); manual chevron is the only collapse path;
   FG/BG swatches stay pinned at top; brush keyboard shortcuts (`[`,
   `]`, `X`, `D`) still update the right widgets.
2. **Clip-to-layer** — add an adjustment over a partly-transparent
   pixel layer; toggle `Ctrl+Alt+G`; confirm the visual gating
   (adjustment visible only over the base's alpha), `LayerRowWidget`
   indent + `↳` glyph appears, `Ctrl+Z` removes the clip, `.txl`
   save+reload preserves the flag. Two stacked clipped adjustments
   over one base both gate by the base.
3. **Properties dock** — open it (it's tab-stacked under LayersPanel
   on the right; click the "Properties" tab); click each adjustment-
   layer thumb (Levels via Ctrl+L, Curves via Ctrl+M, Hue/Sat via
   Ctrl+U, B&C via the Layer menu) — each loads in the dock, sliders
   live-preview during drag, **exactly one undo entry per drag**,
   undo restores cleanly. Click a non-adjustment layer → dock shows
   the empty state.
4. **Regression** — `Ctrl+L` / `Ctrl+M` / `Ctrl+U` still add the right
   adjustment + open it in the dock. M3 `.txl` files (v3) still load
   (back-compat). All M3 keyboard shortcuts unchanged.
5. `ctest --test-dir build` → expect 307 tests passing across 33
   executables.

If everything looks good: **tag `v0.4.0-m4`**, push to remote (with
your explicit go-ahead — I'm not pushing without you), update
`docs/STATUS.md` (M4 ✅), `docs/NEXT.md` (point at M5 kickoff with
smart objects as the leading candidate).

If something's broken: report it; we'll fix and re-verify before
tagging.

Cold-start verification:

```
cmake --build build && ctest --test-dir build
QT_QPA_PLATFORM=offscreen timeout 2 ./build/tuxels
```

Expect 307 passing across 33 executables.

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state (M4 active, S0–S4 done; S5 = DoD).
2. This file — DoD walkthrough checklist + tag instructions.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
4. `cat /home/james/.claude/plans/quirky-napping-koala.md` — full M4 plan.
5. `git log --oneline -20` + `git tag --list` — recent commits and
   shipped tags (`v0.0.1-m0`, `v0.1.0-m1`, `v0.2.0-m2`, `v0.3.0-m3`).
6. `cmake --build build && ctest --test-dir build` — confirm green
   tree (307 passing across 33 executables).
