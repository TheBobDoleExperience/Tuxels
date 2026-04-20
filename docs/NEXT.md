# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M2-S2 — Move tool (V).** S0/S1 landed 2026-04-20 (layer origins + v2
`.txl` + `File → Place…` centered import). Next step wires a dedicated
Move tool that drags the active layer's **origin** — pixels don't
change.

- `src/tools/ToolId.h`: add `Move`.
- `src/tools/MoveTool.{h,cpp}`: on press, snapshot active layer's
  `(originX, originY)` and the press point in doc coords. On move,
  expose the live delta via an extension of `ToolBase` (prefer adding
  `std::optional<QPoint> liveLayerOffset()` rather than introducing a
  new overlay channel). On release, push
  `MoveLayerCommand(layerId, beforeOrigin, afterOrigin)`.
- `src/history/MoveLayerCommand.{h,cpp}` (new): trivial origin swap on
  `apply` / `undo`. Full-recomposite invalidate (movement is rarely
  tile-local).
- `src/ui/CanvasView.cpp`: while a Move tool has a live offset, paint
  the composite with the active layer's origin temporarily overridden
  — reuses the `LayerOverride` hook planned for S3's Free Transform
  preview (so we build the override path once).
- `src/ui/ToolsPanel.{h,cpp}`: Move (V) button; no options row yet.
- `src/app/MainWindow.cpp`: tool wiring + V shortcut + commit pop in
  `onLayerPainted` (promote a `PendingMove` similar to `PendingCrop`).
- Tests: `test_move_layer` — push command, verify composite output
  differs by origin delta only, undo/redo restores origin exactly.

After S2:

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
   tree (expect 145 passing tests as of M2-S1 ship).
7. Pick up "Immediately Next" above.
