# Tuxels — Manual Test Checklist

Catches things the unit tests can't: real Qt event flow, gesture timing,
visual feedback, file-dialog round-trips, performance under realistic
workloads.

Generated 2026-04-30 after the M5→M12 autonomous run. Test the items
relevant to the work you've reviewed; tick checkboxes as you go.

---

## 0. Sanity (5 min — do this first)

- [ ] `cd ~/Tuxels && cmake --build build && ctest --test-dir build` — all 42 executables / 414 cases green at HEAD.
- [ ] `QT_QPA_PLATFORM=offscreen timeout 2 ./build/tuxels` — exits with 143 (SIGTERM from timeout, app started + ran cleanly until killed).
- [ ] Launch `./build/tuxels` normally on the desktop. Sample doc loads (white bg + red square + green disc with Multiply blend). Brush a stroke. Save as `.txl`. Reopen the saved file. Stroke survives.

---

## 1. M6 — Group properties / D&D / multi-select (`v0.6.0-m6`)

### Group properties pane (M6-S0)

- [ ] Open the sample doc. Select a `Group` layer (or use Layer → New Group first). The right-side Properties dock should switch to the Group page showing **name (text field) / blend (combo, includes "Pass Through" first) / opacity (slider 0-100%) / clip-to-below (checkbox)**.
- [ ] Edit name. Press Enter (or Tab away). One undo entry covers the rename.
- [ ] Drag opacity slider mid-range. Live preview updates the canvas. Release mouse → single undo entry.
- [ ] Switch blend: Pass Through → Multiply → Pass Through. Each switch is one undo entry. Composite changes correctly (Pass-Through leaks adjustments out; Multiply isolates).
- [ ] Toggle clip-to-below checkbox. One undo entry; `↳` glyph appears in the row.
- [ ] Switch active layer to a non-group. Properties dock switches to the empty page (or to the bound adjustment pane if it's a Levels/Curves/H-S/B&C).

### Drag-and-drop reorder (M6-S1)

- [ ] Open a doc with several layers including a group. Drag a row a few pixels to start the drag.
- [ ] **Custom drop indicator**:
  - Hover the **top quarter** of a row → green line at the row's top edge.
  - Hover the **bottom quarter** of a row → green line at the bottom edge.
  - Hover the **middle half of a group row** → tinted blue fill + 2px border around the row ("drop INTO group").
  - Middle half of a non-group → same as Below (green line at bottom).
- [ ] Release the drag in each zone; layer ends up at the indicated position.
- [ ] Drag a layer out of a group, into another group, between root and a group. All cross-parent moves work.
- [ ] Drag a group onto its own descendant. Drop is silently rejected (no crash, no stale layer).
- [ ] Undo/Redo each drag — round-trips correctly.

### Multi-select + batch ops (M6-S2)

- [ ] **Shift-click** to select a range of rows; **Ctrl-click** to toggle individual rows. The Document's selection set updates accordingly.
- [ ] Select 2-3 layers (mixed pixel + group OK). Press `Delete`. All selected layers vanish in one undo entry. Ctrl+Z restores all.
- [ ] Select 2-3 layers. Press `Ctrl+G`. All wrap into one new group (children in bottom-up order). Group lands at the topmost selected slot.
- [ ] Select 2-3 layers. Toggle the visibility checkbox of one — the others mirror in lock-step. One undo entry covers all.
- [ ] Select layers across different parents (some at root, some inside a group). Delete batch — the descendant-filter excludes children of selected ancestors so a parent + child don't double-delete.

---

## 2. M7 — Polish (`v0.7.0-m7`)

### Multi-select Move (M7-S0)

- [ ] Select 2 pixel layers. Switch to Move tool (`V`). Drag in the canvas. Both layers' origins shift by the same delta in real-time.
- [ ] Release the drag. Status bar reads "Moved 2 layers". Single Ctrl+Z reverts both.
- [ ] Selection includes a group → group skipped, only its pixel children move (if also selected). Only-pixel-layers semantics.
- [ ] No multi-select active → Move falls back to dragging just the active layer (original M2 behavior).

### Cross-scope Up/Down (M7-S1)

- [ ] Active layer at top of a group. Press the toolbar's Up arrow (or Ctrl+]). Layer pops OUT into the parent scope, just above the group in the panel.
- [ ] Active at bottom of a group. Down → pops out below the group.
- [ ] Active at top of root → status bar reads "Already at top of stack".
- [ ] Active at bottom of root → "Already at bottom of stack".
- [ ] Cascades through nested groups — each Up at the top of an outer group pops one level.

### ToolsPanel section persistence (M7-S3)

- [ ] Collapse the Brush + Bucket sections via their chevrons.
- [ ] Quit Tuxels. Relaunch.
- [ ] Brush + Bucket sections start collapsed; other sections start expanded.
- [ ] Verify the QSettings file at `~/.config/Tuxels/Tuxels.conf` (or similar — Linux uses `XDG_CONFIG_HOME`) contains a `[ToolsPanel/Section]` group with the entries.

### Layer Duplicate (M7-S4)

- [ ] Active a pixel layer. Press `Ctrl+J`. Clone appears just above with " copy" suffix on the name. Paint on the clone — source layer's pixels unaffected (deep-copy via `deepCopyTuxImage`).
- [ ] Active a group with several children. `Ctrl+J` recursively clones the group + all descendants. Each child has a fresh layer id; parent + children all renamed " copy".
- [ ] Active an adjustment layer (Levels / Curves / H-S / B&C). `Ctrl+J` preserves params + mask deep-copy.
- [ ] Ctrl+Z removes the duplicate cleanly.

### In-place rename (M7-S5)

- [ ] **Double-click** the name label of any layer row. The label is replaced by a QLineEdit containing the current name, focused with everything selected.
- [ ] Type a new name. Press `Enter` → commits, label updates.
- [ ] Start an edit, then press `Escape` → reverts; label shows the original name; no undo entry created.
- [ ] Start an edit; click somewhere else (focus loss) → commits.
- [ ] Empty-string commit silently reverts (no nameless layer).

### Layer-row context menu (M7-S6)

- [ ] **Right-click** a layer row. Menu opens with: Duplicate Layer (Ctrl+J), Delete Layer, Rename Layer, Group Layer (only for non-groups), Add Layer Mask (only for pixel without existing mask), Color (submenu of 8 entries with current label checked), Clip Toggle.
- [ ] Right-clicking a layer that's NOT the active one: menu opens, choosing an action (e.g. Duplicate) sets that layer active first, then runs the action. The clicked layer becomes the new sole-selected.
- [ ] Click outside the menu → no-op.
- [ ] Each item invokes the same handler as the equivalent menu / shortcut.

---

## 3. M8 — Color labels / tablet / Rasterize (`v0.8.0-m8`)

### Color labels (M8-S0)

- [ ] Right-click row → Color → Red. A 4-px-wide red stripe appears at the row's left edge.
- [ ] Try each of the 8 colors (None / Red / Orange / Yellow / Green / Blue / Violet / Gray). All visible.
- [ ] Save .txl. Reopen. All color labels survive (`.txl` v6 byte after `clipToBelow`).
- [ ] Open a v5 `.txl` from before this session — color labels load as None (pre-v6 path, gated by `hasColorLabelByte`).

### Tablet pressure (M8-S1)

- [ ] **If you have a Wacom or other Qt-recognized tablet**: paint with the brush at varying pressures. Light strokes produce smaller, more transparent stamps; heavy strokes are full-strength. Cursor ring shrinks/grows mid-stroke (M11-S0 visual hookup).
- [ ] No tablet → mouse strokes are unaffected. Pressure path is gated on `pressure_ < 1 - eps` so the no-tablet code path is bitwise identical to pre-M8.

### Rasterize Layer (M8-S2)

- [ ] Active a `GroupLayer` with 2-3 children, mixed blend modes. Layer → Rasterize Layer. Group disappears; a single PixelLayer takes its slot inheriting blend / opacity / clipToBelow / colorLabel / mask.
- [ ] The composite over the underlying layers is visually unchanged from before rasterize (group's own props applied during compose, just on the new pixel layer).
- [ ] Active a pixel layer → status bar reads "Rasterize currently supports group layers only." No-op.
- [ ] Active an adjustment layer → same status message.
- [ ] Ctrl+Z restores the original group + children.

---

## 4. M9 — Merge Down (`v0.9.0-m9`)

- [ ] Two pixel layers stacked, top has Multiply blend at 50% opacity. Active the top. Press `Ctrl+E`.
- [ ] Top layer disappears; bottom is replaced with a new pixel layer named the same as the original bottom. The new layer's pixels have the Multiply effect baked in.
- [ ] Composite over deeper layers is unchanged.
- [ ] Active is at bottom of its scope → status bar reads "Nothing below to merge into."
- [ ] Active is a group / adjustment → "Merge Down currently supports pixel layers only."
- [ ] Active pixel, but the layer below is a group / adjustment → "Merge Down requires the layer below to be a pixel layer."
- [ ] Ctrl+Z restores both original layers.

---

## 5. M10 — Free Transform on multi-select (`v0.10.0-m10`)

This is the deferred-since-M7 architectural item — exercise it
thoroughly.

### Single-source path (regression check — pre-M10 behavior preserved)

- [ ] Select 1 pixel layer. Ctrl+T. Bbox handles appear. Drag corner → scale; drag interior → translate; drag exterior → rotate; drag pivot dot → moves pivot.
- [ ] Shift while scaling → aspect-locked.
- [ ] Shift while rotating → 15° snap.
- [ ] Press Enter → commits, undo entry "Free Transform". Press Escape (instead) → reverts cleanly.

### Multi-source path

- [ ] Select 2-3 pixel layers (e.g., red square at (10,10)-(30,30) and green at (50,50)-(70,70)).
- [ ] Ctrl+T. The bbox-union (10..70, 10..70) shows handles. Pivot dot at union's center.
- [ ] Drag interior. **Both** layers translate together by the same delta. Scratch overlays update in real-time for both.
- [ ] Drag a corner. Both layers scale around the union's pivot. The closer layer to pivot scales less (in absolute terms) than the farther one.
- [ ] Drag exterior. Both rotate around the pivot. Verify each layer's pixels resample correctly (no garbled alpha; rotation preserves color).
- [ ] Move pivot to a non-center point. Drag corner — scale is now around the new pivot for both layers.
- [ ] Press Enter. Undo stack shows ONE entry: "Free Transform (Layers)". Ctrl+Z reverts both layers' pixels + origins atomically.
- [ ] Cancel via Escape — neither layer's actual pixels changed (overlay-only preview).

### Selection mixing

- [ ] Selection includes a group + a pixel layer. Ctrl+T enters with only the pixel layer (groups are skipped — they have no pixels).
- [ ] Selection includes only adjustment layers. Ctrl+T fails to enter (no pixel sources).

---

## 6. M11 — Pressure-aware cursor + Eyedropper (`v0.11.0-m11`)

### Pressure-aware cursor (M11-S0)

- [ ] **Tablet only**: brush cursor ring shrinks dynamically as you press lighter. Match between ring radius and actual stamp footprint should be visually obvious.
- [ ] Mouse path: ring stays at full brush diameter (pressure defaults to 1.0).

### Eyedropper (M11-S1)

- [ ] Press `I` keyboard shortcut. Cursor changes to PointingHand. Status bar may show the active tool.
- [ ] Click on a colored region of the canvas. Foreground swatch in ToolsPanel updates to that color.
- [ ] Click on a transparent / empty region. Foreground swatch unchanged.
- [ ] Switch back to Brush (`B`). Paint with the picked color.

---

## 7. M12 — PSD import (`v0.12.0-m12`)

### Open path

- [ ] File → Open... → file dialog filter "Tuxels / PSD / PNG (*.txl *.psd *.png)" includes PSD.
- [ ] Open a real `.psd` exported from Photoshop (8-bit RGB):
  - All pixel layers visible in the panel with their original names.
  - Group structure preserved (nested groups land in the right scope).
  - Per-layer opacity + visibility + blend mode correct.
  - Per-layer masks visible (verify by toggling visibility, painting through them).
  - Composite roughly matches PS's render.

### Rejection paths

- [ ] Try a 16-bit-per-channel PSD → status / dialog shows "Only 8-bit-per-channel PSDs supported".
- [ ] Try a CMYK PSD → "Only RGB color mode supported in M12".
- [ ] Try a PSB (Photoshop Big — `.psb` extension or > 30000px dim PSD) → "PSB / large-document files are not supported in M12".
- [ ] Try a non-PSD file with a `.psd` extension renamed → "Not a PSD file (missing 8BPS magic)".

### Known issue (user-reported, 2026-04-30)

- [ ] **Navigation is slow when a real PSD is loaded.** Verify by panning + zooming a non-trivial PSD; timing how long each frame takes. Likely culprits:
  - Per-layer compose cost grows with layer count + per-layer image size.
  - `recompositePartial` may be falling back to full recompose.
  - PSD layers have arbitrary origins/sizes — many tiles may be present but mostly outside the doc rect.
  - Possible fix in M13+: lazy compose at the visible viewport rect; compose-tile parallelism (the `Performance pass` candidate in NEXT.md).

### Unsupported PSD features (silent)

These either rasterize to plain pixel layers or are skipped — verify they don't crash, but don't expect them to behave like in PS:

- [ ] Smart objects → land as pixel layers (their cached composite contribution).
- [ ] Text layers → pixel layers with the text rasterized at PS's saved size.
- [ ] Adjustment layers (PSD's own) → typically appear as transparent pixel layers (PSD doesn't store the adjusted result — would need M13+ adjustment-layer round-trip).
- [ ] Layer effects (drop shadow, glow, stroke) — `lfx2` block is skipped; effects don't render.

---

## 8. Cross-version `.txl` round-trip

After every milestone with a format bump, old files should still load.
Quick spot-check:

- [ ] Open the very first .txl you saved (probably v1 or v2). Loads with `clipToBelow == false`, `colorLabel == None`, no group structure.
- [ ] Open a v4 .txl (M4 era). `clipToBelow` byte is honored.
- [ ] Open a v5 .txl (M5 era). Group section dividers reconstructed.
- [ ] Open a v6 .txl (M8 era). Color labels honored.
- [ ] Save current state → reopen → all current-feature state preserved.

---

## 9. Performance investigation queue (for M13+)

Things noticed during this session that warrant profiling:

- [ ] **PSD navigation slowness** — user-reported. Profile with `perf record -g ./build/tuxels`, load a real PSD, navigate. Hot spots likely in `composeTileRange` or `renderTile` loops.
- [ ] Multi-source Free Transform with many layers: each scratch resample is independent, so this should parallelize cleanly. Single-thread cost grows linearly with N layers — measure for N > 5 layers.
- [ ] Brush dynamics during stylus drag: `BrushEngine::applyStamp` is hot. Pressure-aware path now branches; verify the no-pressure path still measures bitwise-identical to pre-M8 (regression test exists; just ensure release build perf hasn't drifted).
- [ ] `LayersPanel::refresh` rebuilds all rows from scratch on any tree mutation. Becomes O(N) per change. For 50+ layer docs, this could stutter. Consider incremental updates in M13+.

---

## 10. Things that should NOT have broken (regressions to watch)

- [ ] All keyboard shortcuts from M0-M5 still work: `B/M/V/L/P/W/⇧W/C/G/⌃T/I` for tools; `Ctrl+Z/⇧Z`, `Ctrl+S/⇧S`, `Ctrl+A/D/⇧I`, `Ctrl+G/⇧G`, `Ctrl+L/M/U/J/E/T`, `Ctrl+Alt+G`.
- [ ] Adjustment layers (Levels / Curves / Hue-Sat / B&C) edit through the Properties dock without modal dialogs. One undo entry per drag/edit.
- [ ] Crop tool. Free Transform single-layer (regression check above).
- [ ] Lasso, Polygonal Lasso, Magic Wand, Select By Color all still produce expected selection masks.
- [ ] Save As .txl + Open round-trip preserves everything (active layer id, paint target, selection, every layer kind, masks, group nesting, origins).
- [ ] Place Image (Ctrl+Shift+P) still works.
- [ ] Marching ants on the active selection still animate.
