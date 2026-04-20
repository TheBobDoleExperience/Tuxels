# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M1 — S6: `.txl` native file format v1.** S1–S5 all shipped 2026-04-20.
117 tests passing. Next up: design and implement Tuxels' proprietary file
format that round-trips document + layers + masks + selection.

Key decisions to make at S6 start:

1. **Container format**: ZIP-of-binaries (familiar, diff-friendly, but
   platform dependencies) vs custom chunked binary (full control, but
   more code). SCOPE.md hasn't locked this in — pick at S6 kickoff.
2. **Tile storage**: preserve the tile-sparse `TuxImage` layout
   directly (store only present tiles, each with `(tx, ty)` header) vs
   a flat image per layer. Tile-sparse matches how TuxImage already
   works and keeps large mostly-empty masks cheap on disk.
3. **Compression**: zstd for the tile payloads is the default.
4. **Version field**: include in the top-level header so future readers
   can branch. Bump on any tile-format or layer-kind change.

Payload to serialize (derived from current Document model):

- Document dimensions + active layer index + paint target
- Per-layer: id, name, kind (PixelLayer-only for v1), visibility,
  opacity, blend mode (13 enum values), attached mask (bool + tiles +
  enabled), PixelLayer tiles
- Selection mask (optional) — same tile layout, R-channel only

## When Starting S6

1. Decide container + compression. Write the choice into
   `ARCHITECTURE.md` before coding.
2. Add `src/io/TxlIO.{h,cpp}` with `saveTxl` / `loadTxl` mirrors of
   PngIO.
3. Add round-trip tests in `tests/test_txl_io.cpp` (Qt-free — keep in
   `tuxels_core` if the format writer is pure C++; else split like
   PngIO).
4. Wire `File → Save As…` / `File → Open…` to dispatch on extension
   (.txl vs .png).
5. Mark S6 done in STATUS.md, then S7 (tag v0.1.0-m1).

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state.
2. This file — what to do next.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions.
4. `git log --oneline -10` — recent commits.
5. `cmake --build build && ctest --test-dir build` — confirm green tree
   (expect 117 passing tests as of M1-S5).
6. Pick up "Immediately Next" above.
