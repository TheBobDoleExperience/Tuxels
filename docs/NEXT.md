# Tuxels — Next Actions

**Read STATUS.md first for context.**

## Immediately Next

**M12 shipped** as `v0.12.0-m12` on 2026-04-30 (414 cases / 42 execs):
- M12 PSD import (read-only) — header, raw + PackBits RLE, multi-
  layer, group section dividers, per-layer user masks (`9427355`)
- M12-S5 file menu integration + docs + tag

**M11 shipped** as `v0.11.0-m11` (pressure-aware brush cursor +
Eyedropper). **M10 shipped** as `v0.10.0-m10` (Free Transform on
multi-select).

Tags pushed; main pushed.

**Start here: M13 kickoff.** With PSD read landed, the next-biggest
gaps are Smart Objects, Layer Effects, and Text Layers.

## M13 Candidates

1. **Layer effects.** Drop shadow, glow, stroke, inner shadow. Each
   effect is a per-layer pre-pass between renderTile and blend.
   Cross-tile blur is the main complexity (alpha pre-pass needs to
   read N pixels into adjacent tiles). For a first cut, drop shadow
   only with box blur within tile boundaries. Big surface — could
   span 2 milestones.

2. **Text layers.** New `TextLayer` kind. Qt's `QPainter` +
   `QFontMetrics` for glyph layout; rasterize on demand into a
   tile-sparse TuxImage. Whole milestone.

3. **Smart Objects.** Re-editable embedded sub-documents. Recursive
   compose already done. New pieces: .txl extension for embedded
   docs; separate undo stack per child window.

4. **PSD: ZIP compression decoders.** M12 only handles raw + RLE.
   ZIP without prediction (compression == 2) is a straight zlib
   inflate; ZIP with prediction (compression == 3) requires a
   reverse-prediction pass. Pull in zlib via Qt's Qt6::Core, or
   bundle miniz.

5. **PSD: smart-object pixel data.** Currently smart-object records
   flatten to whatever composite the file embeds; PSD also stores
   the original embedded image. Could surface that as a re-editable
   sub-document.

6. **PSD: Layer effects fxrl decoder.** PSD's `lfx2` block carries
   layer-effect params (drop shadow, glow, stroke, etc.). Once M13's
   own Layer Effects land, decode `lfx2` into them.

7. **Performance pass.** Multi-thread `composeTileRange` per tile;
   per-tile GPU upload via QOpenGLTexture; lazy histogram recompute.

8. **Color picker: Alt-eyedropper while in Brush.** PS lets you
   Alt+click while the Brush tool is active to temporarily pick a
   color without switching tools. Small follow-up to M11-S1.

9. **"Isolate adjustments" toggle** for groups. One bool on
   GroupLayer + .txl v7.

10. **Layer search / filter** in LayersPanel. Type-to-filter the row
    list — useful in deeply nested docs.

Cold-start verification:

```
cmake --build build && ctest --test-dir build
QT_QPA_PLATFORM=offscreen timeout 2 ./build/tuxels
```

Expect 42 executables / 414 internal cases at `v0.12.0-m12`.

## Cold-Start Checklist

1. `cat docs/STATUS.md` — current state (M12 ✅ shipped, M13 TBD).
2. This file — M13 kickoff candidates.
3. `cat docs/ARCHITECTURE.md` — don't re-derive decisions:
   - M5 layer tree + recursive compose + .txl v5 sections
   - M6 group properties / D&D / multi-select
   - M7 polish (multi-select Move, cross-scope Up/Down, drop indicator,
     persistence, Layer Duplicate, rename, context menu)
   - M8 color labels (.txl v6) / tablet pressure / Rasterize
   - M9 Merge Down
   - M10 compose span overrides + TransformTool multi-source
   - M11 pressure-aware brush cursor + Eyedropper
   - M12 PSD read (raw + RLE; sections; masks)
4. `git log --oneline -30` + `git tag --list` — recent commits and
   shipped tags (`v0.0.1-m0` through `v0.12.0-m12`).
5. `cmake --build build && ctest --test-dir build` — confirm
   green tree (42 executables / 414 cases at `v0.12.0-m12`).
