# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**S9 — Layer masks in UI**

`LayerBase` already carries a `std::unique_ptr<LayerMask> mask;` field and
`src/layers/LayerMask.h` already defines the struct (TuxImage + enabled
flag). The compositor already applies the mask's red channel as a
multiplier on source alpha (see `src/compositor/compose.cpp`, covered by
`test_compositor.cpp`). So the runtime side is done — S9 is purely UI
wiring to let the user actually make and edit one.

Work items:
- **Menu action**: `Layer → Add Layer Mask`. Creates a white-filled mask
  sized to the document and attaches it to the active layer (initial
  state: fully opaque ⇒ no effect). Wrap in a `LayerOpCommand` so it's
  undoable; undo detaches, redo re-attaches (stash the unique_ptr in the
  command's closure like `onLayerDelete` does).
- **Thumbnail**: `LayerRowWidget` should show a second thumbnail next to
  the layer thumb when a mask is present. Rebuild in `rebuildThumbnail`.
- **Click-to-target**: Clicking the layer thumb vs the mask thumb selects
  the paint target. Hold that state per-row or, better, on the Document
  (`enum class PaintTarget { Layer, Mask };` + `paintTarget()` getter).
  BrushTool reads this and paints into `active_->image` or
  `active_->mask->image` accordingly. The mask is a grayscale
  interpretation but the storage is still `Rgba32F`; brush color for
  mask target should be forced to `{v,v,v,1}` where v is a single value
  (0 = hide, 1 = reveal). Simplest: when painting on a mask, ignore the
  RGB picker and use a single float intensity slider, or just hardcode
  black=hide / white=reveal + eraser flag.
- **Shift-click disables mask**: Toggle `mask->enabled`. Undo via
  LayerOpCommand (bool flip).
- **Right-click menu on mask thumb**: "Delete Mask". Undoable.

Tests (optional):
- `tests/test_mask.cpp` — not needed at the compose level (already
  covered in test_compositor). A small unit for "paint on mask reduces
  source alpha at that region" would be nice but low priority.

Commit: "layers: raster layer masks in UI".

## Then — S10: End-to-end verification + tag v0.0.1-m0

Run through every DoD bullet in
`/home/james/.claude/plans/modular-singing-teacup.md`:
1. Launch tuxels; window appears.
2. File → Open a PNG; layer appears.
3. File → New at custom dimensions.
4. Visibility toggle, opacity slider, blend combo work; thumbnails update.
5. Add / delete / reorder / rename layers.
6. Paint with brush; `[` / `]` resize.
7. Add a layer mask; paint on it; shift-click toggles it; layer result
   reflects mask.
8. Cycle blend modes — each produces the expected composite.
9. Undo / Redo everything above.
10. Export As PNG — open in an external viewer to sanity-check.

Tag `v0.0.1-m0`, push a summary update in LOG.md, and declare M0 done.

Then pick the next milestone (M1). Candidates, ordered by value to user:
- Text layer (so users can annotate exports).
- Selection tools (rectangular / lasso) — cornerstone feature.
- Adjustment layers (HSL, Curves, Levels) — high ROI for demos.
- PSD I/O (slow, read-only first).

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state.
2. This file — what to do next.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
4. `git log --oneline -10` — recent commits.
5. `cmake --build build && ctest --test-dir build` — confirm green tree
   (expect 59 passing tests as of S8).
6. Pick up "Immediately Next" above.
