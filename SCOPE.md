# Tuxels — Project Scope & Technical Blueprint

> *Tuxels* (Tux + Pixels): A professional, non-destructive raster image editor built native for Linux, with future Windows and macOS support. Designed to be a credible Photoshop replacement for working photographers, designers, and print professionals.

---

## Table of Contents

1. [Project Vision](#1-project-vision)
2. [Target Users](#2-target-users)
3. [Technology Stack](#3-technology-stack)
4. [Core Architecture](#4-core-architecture)
5. [Feature Scope](#5-feature-scope)
   - 5.1 [PSD Compatibility](#51-absolute-bulletproof-psd-compatibility)
   - 5.2 [Non-Destructive Editing](#52-true-non-destructive-editing)
   - 5.3 [Smart Selection Tools](#53-advanced-smart-selection-tools)
   - 5.4 [Color & Print Management](#54-professional-color--print-management)
   - 5.5 [UI Customization & Muscle Memory](#55-ui-customization--muscle-memory)
   - 5.6 [Layer Styles](#56-layer-styles-blending-options)
6. [Development Phases](#6-development-phases)
7. [Build System & Dependencies](#7-build-system--dependencies)
8. [Testing Strategy](#8-testing-strategy)
9. [Risks & Open Questions](#9-risks--open-questions)
10. [Success Criteria](#10-success-criteria)

---

## 1. Project Vision

The Linux desktop has mature alternatives for 3D (Blender), vector graphics (Inkscape), and digital painting (Krita), but no raster image editor that a Photoshop professional can switch to without compromise. Tuxels aims to fill that gap.

### Guiding Principles

- **Compatibility over novelty.** A working professional's first question is "Can I open my existing files?" — not "What cool new features do you have?" PSD round-trip fidelity is the single highest priority.
- **Non-destructive by default.** Every operation that *can* be non-destructive *should* be. Pixels are sacred; edits are metadata until the user says otherwise.
- **Familiar on day one.** A Photoshop user should be productive within minutes, not days. Default keybinds, panel layouts, and terminology should map directly to what they already know.
- **Native performance.** No Electron. No browser runtime. The editor must feel instantaneous on a mid-range Linux workstation, even with 500 MB PSDs.
- **Print-ready from the ground up.** CMYK, ICC profiles, and high bit-depth are not afterthoughts bolted on later — they are baked into the image pipeline from the very first line of code.

---

## 2. Target Users

| Persona | Needs | Current Pain |
|---------|-------|--------------|
| **Photographer** | RAW development, retouching, batch export, ICC-accurate soft proofing | Darktable/RawTherapee handle RAW but hand off to GIMP for compositing, which lacks non-destructive layers |
| **Graphic Designer** | Layout, layer styles, PSD collaboration with clients on Adobe | GIMP cannot round-trip PSDs reliably; Krita is painting-focused |
| **Print Professional** | CMYK, spot colors, high bit-depth, color profiles | Almost no Linux tool handles native CMYK editing |
| **UI/UX Designer** | Layer styles, vector shapes, export slices, design tokens | Figma (browser) works but is not local/native |
| **Photo Retoucher** | Content-aware fill, frequency separation, advanced masking | GIMP's selection tools and healing brush are a generation behind |

---

## 3. Technology Stack

### Language: C++20

- Mature ecosystem for image processing, GPU compute, and GUI frameworks.
- Direct access to SIMD intrinsics for pixel-level performance.
- Extensive existing libraries (LittleCMS, OpenEXR, Qt).

### GUI Framework: Qt 6 (Widgets + QGraphicsView)

- Native look and feel on Linux (KDE integration, but works on GNOME/Wayland/X11).
- Built-in dockable/detachable panel system (`QDockWidget`).
- Cross-platform path to Windows and macOS with minimal porting effort.
- Hardware-accelerated canvas via `QOpenGLWidget` or Qt RHI.
- Dual-license: GPL for open-source, commercial license available if needed.

### Color Management: LittleCMS 2 (lcms2)

- Industry-standard open-source ICC profile engine.
- Supports ICC v4, CMYK, Lab, multi-profile transforms.
- Used by GIMP, Scribus, Firefox, and Krita.

### Color Pipeline (optional future): OpenColorIO (OCIO)

- ACES and film-industry color workflows.
- Useful for VFX/film crossover users.

### Image I/O

| Format | Library | Notes |
|--------|---------|-------|
| PSD | Custom engine (see [5.1](#51-absolute-bulletproof-psd-compatibility)) | Built on Adobe's published spec + community reverse-engineering |
| PNG | libpng | 8/16-bit |
| JPEG | libjpeg-turbo | SIMD-accelerated |
| TIFF | libtiff | CMYK, 16/32-bit, multi-page |
| WebP | libwebp | Modern web export |
| OpenEXR | OpenEXR | 32-bit HDR |
| RAW | LibRaw | Camera RAW import |
| SVG | Qt SVG / resvg | Vector smart object import |
| JPEG XL | libjxl | Next-gen lossy/lossless |

### GPU Compute

- **OpenCL** or **Vulkan Compute** for filter acceleration and canvas compositing.
- Fallback to CPU (SIMD-optimized) on systems without GPU support.

### ML / AI Inference (for smart tools)

- **ONNX Runtime** for running pre-trained models (segmentation, inpainting).
- Models ship as separate downloadable assets to keep the base install lean.

---

## 4. Core Architecture

### 4.1 Document Model

The document model is the heart of Tuxels. Every design decision flows from it.

```
TuxDocument
├── Canvas (width, height, resolution, color mode, bit depth, ICC profile)
├── LayerTree (ordered, nestable)
│   ├── PixelLayer
│   │   ├── pixel data (tiles)
│   │   ├── opacity, blend mode
│   │   ├── layer mask (grayscale tile data)
│   │   ├── vector mask (path data)
│   │   ├── clipping mask flag (clips to layer below)
│   │   └── layer styles (list of live effects)
│   ├── AdjustmentLayer
│   │   ├── type (Curves, Levels, Hue/Sat, Color Balance, ...)
│   │   ├── parameters (non-destructive, re-editable)
│   │   ├── layer mask
│   │   └── clipping mask flag
│   ├── GroupLayer
│   │   ├── children (recursive LayerTree)
│   │   ├── blend mode, opacity
│   │   ├── layer mask
│   │   └── pass-through vs. normal blending
│   ├── SmartObjectLayer
│   │   ├── embedded source (original pixels or linked file)
│   │   ├── transform stack (scale, rotate, warp)
│   │   ├── smart filter stack (non-destructive filters)
│   │   └── layer mask
│   └── TextLayer
│       ├── rich text data (font, size, color, kerning, paragraph)
│       ├── rasterization cache
│       └── layer styles
├── Channels (composite + individual R/G/B/A or C/M/Y/K + spot colors)
├── Paths (vector path data, for clipping paths and shape layers)
├── Selection (marching ants — stored as a grayscale mask)
└── History (undo/redo stack, branching)
```

### 4.2 Tiled Image Storage

Large images (100+ megapixels) cannot live in a single contiguous buffer. Tuxels uses a **tiled architecture**:

- Images are divided into fixed-size tiles (e.g., 256x256 pixels).
- Only tiles that contain non-transparent data are allocated (sparse storage).
- Tiles are the unit of undo/redo (copy-on-write), GPU upload, caching, and disk swap.
- Mipmap pyramid for fast zoom-out rendering.

### 4.3 Compositing Pipeline

```
For each tile in the output:
  1. Walk the layer tree bottom-to-top.
  2. For each layer:
     a. Fetch the layer's tile (or transparent if sparse).
     b. Apply layer mask (multiply alpha by mask grayscale).
     c. Apply vector mask (intersect).
     d. Apply layer styles (drop shadow, glow, stroke, etc.).
     e. If clipping mask: use alpha of the layer below as a mask.
     f. Composite onto the accumulator using the layer's blend mode.
  3. For adjustment layers:
     a. Apply the color transform to the accumulated result so far.
     b. Masked by the adjustment layer's own mask.
  4. For groups:
     a. If pass-through: layers composite directly into the parent stack.
     b. If normal: composite group into an intermediate buffer first, then blend.
  5. Convert from working color space to display color space (via lcms2).
  6. Render the final tile to the viewport.
```

This pipeline must run at **interactive frame rates** (30+ FPS) for the visible region while editing. GPU acceleration is critical.

### 4.4 Undo/Redo System

- **Command pattern**: Every user action is a `Command` object that can `execute()` and `undo()`.
- **Tile-level deltas**: For paint strokes, only the modified tiles are snapshot (copy-on-write), not the entire layer.
- **History panel**: Visual list of all actions, click any point to jump back.
- **History branching** (stretch goal): Non-linear undo, like a git tree.

### 4.5 Plugin / Scripting Architecture (Future)

- **Python scripting** via embedded Python (pybind11) — equivalent to Photoshop's ExtendScript/UXP.
- **C plugin API** for performance-critical filters.
- **8bf plugin compatibility** (stretch goal, via a compatibility shim — this is how PaintShop Pro and others support PS plugins on Windows).

---

## 5. Feature Scope

### 5.1 Absolute, Bulletproof PSD Compatibility

**Why this is first:** If a professional cannot open their archived `.psd` files exactly as they left them, they will not switch. Period.

#### 5.1.1 PSD Read

The PSD format is documented by Adobe in a [public specification](https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/), but the spec is incomplete and sometimes inaccurate. A production-quality reader requires the spec *plus* extensive reverse engineering.

**Must support on read:**

| PSD Feature | Priority | Difficulty | Notes |
|-------------|----------|------------|-------|
| Pixel layers (8/16/32-bit, RGB/CMYK/Grayscale/Lab) | P0 | Medium | Core image data, multiple compression types (raw, RLE, ZIP) |
| Layer ordering, naming, visibility | P0 | Low | Basic layer tree reconstruction |
| Layer opacity & blend modes (all 27+) | P0 | Medium | Must match Photoshop's exact blending math |
| Layer masks (raster) | P0 | Medium | Grayscale masks with default/density/feather parameters |
| Clipping masks | P0 | Low | Flag on the layer record |
| Layer groups (folders) | P0 | Medium | Nested via "section divider" markers in the layer list |
| Adjustment layers (all types) | P1 | Hard | Each type has its own binary descriptor format |
| Layer styles (drop shadow, stroke, etc.) | P1 | Hard | Complex descriptor-based serialization |
| Text layers | P1 | Hard | Rich text with full typography info, font matching |
| Vector masks / shape layers | P1 | Hard | Bezier path data, fill/stroke rules |
| Smart Objects | P2 | Very Hard | Embedded PSD-within-a-PSD or linked external files |
| Channels (alpha, spot) | P1 | Medium | Extra channel data beyond RGBA |
| Paths | P1 | Medium | Named bezier paths, clipping paths |
| Guides, grids, slices | P2 | Low | Metadata in the image resources section |
| Color profiles (embedded ICC) | P0 | Low | Just extract and hand to lcms2 |

**Strategy:** Build the PSD reader incrementally, tested against a corpus of real-world PSD files. Prioritize *visual fidelity* — when an unsupported feature is encountered, degrade gracefully (flatten to pixels) rather than crash or silently drop data.

#### 5.1.2 PSD Write

The write path is equally important for collaboration with Adobe users.

- **Round-trip fidelity**: Open a PSD, make no changes, save — the output should produce the same visual result when opened in Photoshop.
- **Supported on write**: Everything supported on read, written back in standard PSD format.
- **Compatibility mode**: Option to flatten unsupported features on export (with a warning dialog) to ensure the file always opens in Photoshop.
- **PSB (Large Document Format)**: Support for documents over 30,000 x 30,000 pixels or 2 GB, using the `.psb` extension.

#### 5.1.3 Existing Open-Source PSD Work

The PSD format is the single hardest part of this project. Fortunately, there are thousands of hours of open-source work already done on parsing it. Tuxels should study and selectively build on this knowledge rather than starting from zero.

**Worth leveraging:**

| Project | Language | License | Value to Tuxels |
|---------|----------|---------|-----------------|
| **GIMP `file-psd` plugin** | C | GPL v3 | Years of battle-tested PSD read/write code. Handles layers, masks, blend modes, and many undocumented format quirks. Weak on text layers and smart objects, but solid on the fundamentals. If Tuxels is GPL v3, code can be studied and adapted directly. |
| **Krita PSD plugin** | C++ | GPL v3 | Newer, arguably cleaner PSD implementation than GIMP's. Also C++ (same as Tuxels), so more directly portable. Worth comparing against GIMP's approach. |
| **`psd-tools`** | Python | MIT | Excellent *reference implementation* — very well-documented, easy to read. Not useful for shipping code (wrong language), but invaluable for understanding the format structure, descriptor parsing, and undocumented fields. |
| **Adobe PSD Spec** | — | Public | The official specification. Incomplete and sometimes inaccurate, but the starting point. |

**Not worth borrowing:**

- GIMP's or Krita's UI code, application architecture, or tool implementations. Both projects have fundamentally different design philosophies from Tuxels. GIMP's document model in particular lacks the non-destructive pipeline Tuxels requires. The goal is a Photoshop-like experience, not a GIMP-like one.
- The value here is strictly in **format parsing** — the hard-won knowledge of how PSD files actually work in the wild, beyond what Adobe's spec documents.

**Approach:** Start by studying all three implementations in parallel. Use `psd-tools` to build understanding of the format, cross-reference with GIMP and Krita's C/C++ code for real-world edge cases, then write Tuxels' own PSD engine informed by all of them.

#### 5.1.4 PSD Test Corpus

Maintain a library of reference PSD files:

- Simple (flat layers, basic masks)
- Complex (nested groups, all blend modes, adjustment layers, text, styles)
- Edge cases (CMYK, 16-bit, 32-bit HDR, spot colors)
- Real-world (open-source PSD templates, community-contributed files)
- Each file has a reference rendering (from Photoshop) to diff against.

---

### 5.2 True Non-Destructive Editing

#### 5.2.1 Adjustment Layers

Adjustment layers apply a color/tone transformation to all layers below them (within their group), without modifying any pixel data.

**Required adjustment types:**

| Adjustment | Description | Complexity |
|------------|-------------|------------|
| Brightness/Contrast | Simple linear adjustment | Low |
| Levels | Per-channel input/output histogram mapping | Medium |
| Curves | Per-channel spline-based tone mapping | Medium |
| Exposure | EV-based, gamma, offset (for 32-bit HDR) | Low |
| Vibrance | Saturation that protects skin tones | Medium |
| Hue/Saturation | Per-range hue/sat/lightness shift | Medium |
| Color Balance | Shadow/midtone/highlight color shift | Medium |
| Black & White | Channel-based luminance mixing | Low |
| Photo Filter | Simulated lens color filter | Low |
| Channel Mixer | Per-channel cross-mixing | Low |
| Selective Color | CMYK-based per-color adjustment | Medium |
| Gradient Map | Luminance-to-gradient color mapping | Medium |
| Posterize | Bit-depth reduction | Low |
| Threshold | Binary black/white at a luminance threshold | Low |
| Invert | Negate all channels | Trivial |

Each adjustment layer:
- Has its own **properties panel** for interactive editing.
- Can be **masked** (paint on the mask to limit where the adjustment applies).
- Can be **clipped** to the layer directly below.
- Stores only parameters (a few bytes), not pixel data.
- Is **re-editable** at any time — double-click to reopen its properties.

#### 5.2.2 Smart Objects

A Smart Object is a layer that wraps an original source (pixel data, a vector file, or even another PSD) in a protective container.

**Core behaviors:**

- **Transform without loss**: Scale down to 10%, then scale back to 100% — the pixels are derived from the original, not from the scaled version. This is the killer feature.
- **Smart Filters**: Apply any filter (blur, sharpen, distort, etc.) to a Smart Object. The filter is stored as metadata, re-editable, with its own mask. The original pixels are never touched.
- **Edit source**: Double-click a Smart Object to open its contents in a separate tab/window. Save to update all instances.
- **Linked vs. Embedded**: Embedded stores the source inside the document. Linked references an external file (and updates when that file changes).
- **Multiple instances**: Place the same Smart Object multiple times — editing the source updates all instances.

**Implementation approach:**

- Internally, a Smart Object is a `TuxDocument` embedded within a layer.
- The transform stack and smart filter stack are applied at compositing time.
- The source document is rendered into a cache at the current resolution; the cache is invalidated when the source or transforms change.

#### 5.2.3 Clipping Masks

Clipping masks use the transparency (alpha channel) of one layer to define the visible region of the layer(s) above it.

- The **base layer** defines the clip shape.
- One or more layers above it are "clipped" (indicated by an indent/arrow in the layer panel).
- Implementation: During compositing, the clipped layer's alpha is intersected with the base layer's alpha.
- Must support chaining: Multiple layers can be clipped to the same base.
- Must interact correctly with blend modes and layer styles on the clipped layers.

---

### 5.3 Advanced, "Smart" Selection Tools

#### 5.3.1 Basic Selection Tools (P0)

These must exist and work flawlessly before the smart tools matter:

- **Rectangular Marquee** — with fixed size, fixed ratio, and feathering.
- **Elliptical Marquee** — same options.
- **Lasso** (freehand), **Polygonal Lasso** (click-to-click), **Magnetic Lasso** (edge-snapping).
- **Magic Wand** — contiguous and non-contiguous, with tolerance.
- **Color Range** — select by sampled color with fuzziness control.
- **Select All**, **Deselect**, **Inverse**, **Grow**, **Similar**, **Modify** (expand, contract, feather, smooth).
- **Quick Mask mode** — paint the selection as a red overlay.
- Selection **operations**: new, add (Shift), subtract (Alt), intersect (Shift+Alt).

#### 5.3.2 Quick Selection & Object Selection (P1)

- **Quick Selection Tool**: Paint over the subject; the tool auto-expands to edges. Uses local edge detection and region growing.
- **Object Selection Tool**: Draw a rough rectangle or lasso around an object; the tool automatically finds the object's boundary. Powered by a segmentation model (see below).

#### 5.3.3 Select Subject / Select and Mask (P1)

- **Select Subject**: One-click whole-subject selection. Uses an ML segmentation model (e.g., Meta's SAM — Segment Anything Model) running locally via ONNX Runtime.
- **Select and Mask workspace**: A dedicated mode for refining selections, especially around complex edges:
  - **Refine Edge Brush**: Paint along hair/fur/leaves; the algorithm uses matting techniques (alpha matting, deep matting) to compute sub-pixel-accurate transparency.
  - **Edge detection controls**: Radius, Smart Radius, Smooth, Feather, Contrast, Shift Edge.
  - **Preview modes**: Marching ants, overlay, on black, on white, black & white mask, on layer.
  - **Output options**: Selection, layer mask, new layer, new layer with mask.
  - **Decontaminate Colors**: Removes color fringe from edge pixels by replacing them with nearby subject colors.

#### 5.3.4 Content-Aware Fill (P2)

Remove unwanted objects from an image by intelligently synthesizing replacement pixels from the surrounding area.

**Approach: Dual-engine**

1. **Classical engine (PatchMatch):** The algorithm behind Photoshop's original Content-Aware Fill. Iteratively finds nearest-neighbor patches in the source image to fill the target region. Fast, deterministic, no GPU required.
   - Good for: Textures, repeating patterns, simple backgrounds.

2. **ML engine (LaMa / MAT / similar):** A pre-trained deep inpainting model. Runs on GPU via ONNX Runtime. Produces more convincing results for complex scenes (faces, perspective, heterogeneous backgrounds).
   - Ships as an optional model download.
   - Falls back to PatchMatch if the model is not installed or no GPU is available.

**User workflow:**
1. Select the area to remove.
2. Open Content-Aware Fill workspace.
3. Adjust the sampling region (where the algorithm can pull texture from).
4. Preview the result in real time.
5. Output to current layer or a new layer.

---

### 5.4 Professional Color & Print Management

#### 5.4.1 Color Modes

Tuxels must support multiple color modes as **first-class working spaces**, not just for import/export:

| Mode | Channels | Use Case |
|------|----------|----------|
| **RGB** | R, G, B (+ Alpha) | Screen work, web, photography |
| **CMYK** | C, M, Y, K (+ Alpha) | Print production |
| **Grayscale** | Gray (+ Alpha) | Black and white photography, masks |
| **Lab** | L, a, b (+ Alpha) | Color science, perceptual editing |

- Mode conversion via lcms2 with ICC profile awareness.
- All tools, filters, blending modes, and adjustment layers must work in all color modes. This is a significant engineering constraint — it's tempting to hard-code RGB assumptions everywhere, but **CMYK must not be a second-class citizen**.

#### 5.4.2 Bit Depth

| Depth | Storage | Use Case |
|-------|---------|----------|
| **8-bit** | 0–255 per channel | Standard, web, final output |
| **16-bit** | 0–65535 per channel | Heavy retouching without banding |
| **32-bit** | float per channel | HDR imaging, compositing, VFX |

- The internal compositing pipeline should **always operate in at least 32-bit float** for maximum precision, even when the document is 8-bit. Final quantization happens at display time or export.
- 32-bit mode enables HDR merging, EV-based exposure adjustments, and linear-light compositing.

#### 5.4.3 ICC Color Management

- **Every document has an assigned ICC profile** (e.g., sRGB IEC61966-2.1, Adobe RGB 1998, ProPhoto RGB, SWOP CMYK, Fogra39, etc.).
- **Display profile**: Tuxels reads the system's monitor ICC profile (via colord on Linux) and transforms all display output through it.
- **Soft proofing**: Simulate how the image will look when printed on a specific device, including rendering intent (perceptual, relative colorimetric, absolute colorimetric, saturation) and black point compensation.
- **Gamut warning**: Highlight pixels that are out-of-gamut for the proof profile.
- **Assign vs. Convert**: Assign changes the profile interpretation without altering pixels; Convert recalculates pixel values to match the new profile.
- **Color Settings dialog**: Global defaults for RGB, CMYK, Gray, and spot working spaces, plus profile mismatch and missing profile policies.

#### 5.4.4 Spot Colors (Stretch Goal)

- Support for spot color channels (Pantone, custom ink).
- Required for packaging and specialty printing.
- Spot channels render as an overlay in the composite view.

---

### 5.5 UI Customization & Muscle Memory

#### 5.5.1 First-Run Experience

On first launch, Tuxels presents a brief setup wizard:

1. **Keyboard shortcut preset**: "Photoshop-style (recommended for PS users)" or "Tuxels native" or "Custom."
2. **Color theme**: Dark (default), Light, System.
3. **Workspace layout preset**: "Photography", "Design", "Painting", or "Default."
4. **UI scale**: Auto-detect from system DPI, with manual override.

#### 5.5.2 Keyboard Shortcuts

**Photoshop-default mappings (partial list, for reference):**

| Key | Tool / Action |
|-----|---------------|
| `V` | Move |
| `M` | Marquee |
| `L` | Lasso |
| `W` | Quick Selection / Magic Wand |
| `C` | Crop |
| `I` | Eyedropper |
| `J` | Healing Brush / Patch |
| `B` | Brush |
| `S` | Clone Stamp |
| `E` | Eraser |
| `G` | Gradient / Paint Bucket |
| `O` | Dodge / Burn / Sponge |
| `P` | Pen |
| `T` | Type |
| `U` | Shape |
| `H` | Hand (pan) |
| `Z` | Zoom |
| `D` | Default colors (black/white) |
| `X` | Swap foreground/background |
| `Ctrl+T` | Free Transform |
| `Ctrl+J` | Duplicate Layer |
| `Ctrl+Shift+N` | New Layer |
| `Ctrl+G` | Group Layers |
| `[` / `]` | Brush size down / up |

- Every shortcut is **fully customizable** in Preferences.
- Support for **multi-key chords** (e.g., Shift+Alt+Ctrl combinations).
- **Search/filter** in the keybind editor.
- **Export/import** keybind profiles.
- **Conflict detection** with clear resolution UI.

#### 5.5.3 Dockable / Detachable Panels

Using Qt's `QDockWidget` infrastructure:

- All panels can be **docked** (snapped to edges/subdivided), **tabbed** (stacked with other panels), or **floated** (detached as independent windows — ideal for multi-monitor setups).
- Panels include:
  - Layers, Channels, Paths
  - Properties (context-sensitive: shows adjustment params, layer style editor, etc.)
  - Color (color picker, sliders, swatches)
  - Brush Settings
  - History
  - Navigator (thumbnail overview + zoom box)
  - Info (pixel coordinates, color values under cursor)
  - Actions (recorded macros)
  - Tool Options bar (top, below menu bar)
- **Workspace presets**: Save and restore complete panel layouts by name.
- **Collapse to icons**: Panels can be collapsed to a narrow icon strip on the edge (like Photoshop's minimized panel mode).

#### 5.5.4 Canvas Navigation

- **Zoom**: Scroll wheel, `Ctrl++`/`Ctrl+-`, fit-to-window, actual pixels, percentage entry.
- **Pan**: Middle-click drag, spacebar + drag, scroll bars.
- **Rotate canvas**: `R` + drag (non-destructive display rotation for drawing comfort).
- **Bird's-eye view**: Hold `H` to zoom out momentarily, release to snap back.
- **Multi-touch**: Pinch-to-zoom, two-finger pan (for touchscreen/trackpad users).

---

### 5.6 Layer Styles (Blending Options)

Layer styles are **live, non-destructive effects** attached to a layer. They update automatically when the layer's content changes.

#### 5.6.1 Supported Effects

| Effect | Description |
|--------|-------------|
| **Drop Shadow** | External shadow cast behind the layer. Controls: color, opacity, angle, distance, spread, size (blur). |
| **Inner Shadow** | Shadow inside the layer's edges. Same controls as Drop Shadow. |
| **Outer Glow** | Soft glow radiating outward from the layer's edges. Controls: color or gradient, opacity, technique (softer/precise), spread, size. |
| **Inner Glow** | Soft glow radiating inward. Same controls as Outer Glow + source (center/edge). |
| **Bevel & Emboss** | Simulated 3D beveling. Controls: style (outer/inner/emboss/pillow), technique, depth, direction, size, soften, shading angle/altitude, highlight/shadow mode. |
| **Satin** | Internal shading for a satin-like finish. Controls: blend mode, color, opacity, angle, distance, size, invert. |
| **Color Overlay** | Flat color fill over the layer. Controls: color, blend mode, opacity. |
| **Gradient Overlay** | Gradient fill over the layer. Controls: gradient, blend mode, opacity, style (linear/radial/angle/reflected/diamond), angle, scale. |
| **Pattern Overlay** | Tiled pattern fill. Controls: pattern, blend mode, opacity, scale, link with layer. |
| **Stroke** | Border around the layer's edges. Controls: size, position (outside/inside/center), blend mode, opacity, fill type (color/gradient/pattern). |

#### 5.6.2 Layer Styles Behaviors

- Each effect can be individually **enabled/disabled** without removing its settings.
- **Multiple instances**: In modern Photoshop, you can add multiple drop shadows, etc. Tuxels should support this from the start.
- **Copy/paste styles**: Right-click a layer to copy its styles and paste them onto another layer.
- **Save as preset**: Store a combination of styles as a named preset for reuse.
- **Scale effects**: Scale all effect sizes proportionally (useful when resizing a layer).
- **"Create Layer"**: Option to rasterize individual effects into their own layers for manual editing.
- **Global Light**: A shared light angle that all effects can optionally follow, so shadows are consistent across layers.
- **Blending Options (advanced)**: The "Blend If" sliders that control compositing based on luminosity of the current layer or underlying layers. This is essential for advanced compositing.

#### 5.6.3 Rendering Order

Layer styles render in a specific, fixed order around the layer content. The Photoshop rendering order (which we must match for PSD compatibility) is:

1. Bevel & Emboss (+ Contour, Texture)
2. Inner Shadow
3. Inner Glow
4. Satin
5. Color Overlay
6. Gradient Overlay
7. Pattern Overlay
8. Outer Glow
9. Drop Shadow
10. Stroke (position-dependent)

---

## 6. Development Phases

### Phase 1 — Foundation (Months 1–3)

**Goal:** A working application that can open an image, display it on a GPU-accelerated canvas, and paint on it with a brush.

- [ ] Project scaffolding: CMake build system, CI, linting, test harness.
- [ ] Qt 6 application shell: main window, menu bar, status bar, dockable panel framework.
- [ ] Tiled image buffer (`TuxImage`): sparse tile storage, 8/16/32-bit, RGB + CMYK.
- [ ] Compositing pipeline (CPU, single-threaded, basic): layer stacking with opacity and basic blend modes.
- [ ] Basic layer panel: reorder, rename, visibility toggle, opacity slider.
- [ ] Canvas widget: GPU-accelerated display, zoom, pan.
- [ ] Brush engine: round brush, size, opacity, hardness, pressure sensitivity (Wacom/libinput).
- [ ] Basic tools: brush, eraser, move, rectangular marquee, fill.
- [ ] File I/O: Open/save PNG, JPEG, TIFF. Open PSD (pixel layers only, RGB 8-bit).
- [ ] Undo/redo: command-based, tile-level snapshots.
- [ ] Color management: assign/convert profiles, lcms2-based display transform.
- [ ] Native Tuxels document format (`.tux`): lossless save/load of the full document model.

### Phase 2 — Layer Power (Months 4–6)

**Goal:** Non-destructive editing pipeline and PSD compatibility that covers 80% of real files.

- [ ] Layer masks: paint, view, enable/disable, unlink from layer.
- [ ] Clipping masks.
- [ ] Layer groups with pass-through and normal blending.
- [ ] All 27+ Photoshop blend modes, pixel-accurate.
- [ ] Adjustment layers: Levels, Curves, Hue/Saturation, Brightness/Contrast, Color Balance, Black & White, Invert, Posterize, Threshold.
- [ ] Properties panel for adjustment layer editing.
- [ ] PSD reader: layer masks, groups, blend modes, adjustment layers, clipping masks.
- [ ] PSD writer: round-trip support for all features readable.
- [ ] Layer styles: Drop Shadow, Inner Shadow, Outer Glow, Inner Glow, Stroke, Color Overlay, Gradient Overlay.
- [ ] Free Transform: move, scale, rotate, skew, with bounding-box handles.

### Phase 3 — Selection & Retouching (Months 7–9)

**Goal:** Professional-grade selection and healing tools.

- [ ] All basic selection tools: rectangular, elliptical, lasso (all three), magic wand, color range.
- [ ] Selection operations: add, subtract, intersect, feather, expand, contract, smooth.
- [ ] Quick Mask mode.
- [ ] Quick Selection tool (edge-aware brush).
- [ ] Select Subject (ML-based, SAM model integration via ONNX Runtime).
- [ ] Select and Mask workspace: refine edge brush, view modes, output options.
- [ ] Clone Stamp tool.
- [ ] Healing Brush & Spot Healing Brush.
- [ ] Patch tool.
- [ ] Content-Aware Fill: PatchMatch engine + optional ML inpainting model.
- [ ] Dodge, Burn, Sponge tools.
- [ ] Crop & Perspective Crop tools.
- [ ] Eyedropper, Color Sampler, Ruler, Note tools.

### Phase 4 — Text, Vectors, & Styles (Months 10–12)

**Goal:** Complete the feature set for graphic design workflows.

- [ ] Text tool: rich text editing on canvas, character and paragraph panels.
- [ ] Font management: system font enumeration, font preview.
- [ ] Text layers in PSD: read and write with fidelity.
- [ ] Pen tool: bezier paths, path editing (direct selection, anchor point tools).
- [ ] Shape layers: rectangle, ellipse, polygon, line, custom shape.
- [ ] Vector masks.
- [ ] Layer styles: Bevel & Emboss, Satin, Pattern Overlay. Full Blending Options ("Blend If").
- [ ] Layer styles: presets, copy/paste, scale.
- [ ] Gradient editor (linear, radial, angle, reflected, diamond).
- [ ] Pattern editor.
- [ ] Remaining adjustment layers: Vibrance, Exposure, Photo Filter, Channel Mixer, Selective Color, Gradient Map.

### Phase 5 — Smart Objects & Polish (Months 13–15)

**Goal:** Smart Objects, GPU compositing, performance optimization.

- [ ] Smart Objects: embed/link, transform without loss, edit source.
- [ ] Smart Filters: non-destructive filter stack on Smart Objects.
- [ ] GPU-accelerated compositing pipeline (OpenCL or Vulkan Compute).
- [ ] Multi-threaded CPU compositing fallback.
- [ ] Large document handling (PSB): 300,000+ pixels per dimension.
- [ ] Performance profiling and optimization: 500 MB PSD opens in < 5 seconds, brush at 60 FPS.
- [ ] Batch export (export layers/artboards as individual files).
- [ ] Actions/macros: record and replay sequences of operations.
- [ ] Photoshop-default keybind preset, finalized and tested.
- [ ] Print dialog with soft proof preview, color separation controls.

### Phase 6 — Beta & Community (Months 16–18)

**Goal:** Public beta, PSD compatibility testing at scale, community feedback.

- [ ] Public beta release (AppImage, Flatpak, .deb/.rpm).
- [ ] PSD compatibility testing against a large corpus, bug fixes.
- [ ] User documentation and tutorials.
- [ ] Plugin API (Python scripting).
- [ ] Community feedback integration.
- [ ] Windows build (Qt makes this feasible with moderate effort).
- [ ] Website and download infrastructure.
- [ ] Splash screen, About dialog, first-run wizard.

---

## 7. Build System & Dependencies

### Build System: CMake

```
tuxels/
├── CMakeLists.txt              # Top-level build
├── src/
│   ├── app/                    # Application entry, main window
│   ├── core/                   # Document model, image buffers, tiling
│   ├── compositor/             # Compositing pipeline, blend modes
│   ├── layers/                 # Layer types (pixel, adjustment, smart object, text, group)
│   ├── tools/                  # Brush, selection, transform, crop, etc.
│   ├── psd/                    # PSD/PSB reader and writer
│   ├── io/                     # Other format I/O (PNG, TIFF, JPEG, etc.)
│   ├── color/                  # Color management, ICC, mode conversion
│   ├── ui/                     # Qt widgets, panels, dialogs
│   ├── gpu/                    # OpenCL/Vulkan compute kernels
│   ├── ml/                     # ONNX Runtime inference wrappers
│   ├── filters/                # Image filters (blur, sharpen, distort, etc.)
│   ├── selection/              # Selection engine, marching ants, masking
│   ├── text/                   # Text layout, font management
│   ├── styles/                 # Layer style effects engine
│   └── scripting/              # Python scripting bridge
├── tests/                      # Unit and integration tests
├── resources/                  # Icons, default brushes, patterns, keybind presets
├── third_party/                # Vendored or cmake-fetched dependencies
└── docs/                       # Developer documentation
```

### Required Dependencies

| Library | Purpose | License |
|---------|---------|---------|
| Qt 6 (Widgets, Gui, OpenGL) | GUI framework | LGPL v3 / Commercial |
| LittleCMS 2 | ICC color management | MIT |
| libpng | PNG I/O | libpng license |
| libjpeg-turbo | JPEG I/O | BSD-like |
| libtiff | TIFF I/O | MIT-like |
| libwebp | WebP I/O | BSD |
| OpenEXR / Imath | EXR I/O, HDR | BSD |
| LibRaw | Camera RAW import | LGPL |
| zlib | Compression (PSD uses it) | zlib license |
| ONNX Runtime | ML model inference | MIT |
| FreeType / HarfBuzz | Font rendering and shaping | FreeType / MIT |
| ICU | Unicode text handling | Unicode license |

### Optional Dependencies

| Library | Purpose | License |
|---------|---------|---------|
| OpenColorIO | Film/VFX color pipeline | BSD |
| OpenCL ICD | GPU compute | Vendor-specific |
| Vulkan SDK | GPU compute (alternative) | Apache 2.0 |
| pybind11 | Python scripting | BSD |
| libjxl | JPEG XL I/O | BSD |
| resvg | SVG rendering | MPL 2.0 |

---

## 8. Testing Strategy

### 8.1 Unit Tests

- Every blend mode tested against reference outputs (generated from Photoshop or verified manually).
- Adjustment layer math tested against known input/output pairs.
- PSD reader tested per-feature against reference files.
- Tile manager: allocation, deallocation, copy-on-write correctness.
- Color conversion: RGB ↔ CMYK ↔ Lab round-trip accuracy within acceptable tolerance.

### 8.2 Integration Tests

- **PSD round-trip**: Open a PSD, save it, reopen it, diff the layer tree and pixel data.
- **Visual regression**: Render a set of reference documents and compare output images against baselines (pixel diff with tolerance for floating-point variance).
- **Compositing accuracy**: Render complex layer stacks and compare against Photoshop's output.

### 8.3 Performance Benchmarks

- Brush paint latency (target: < 8 ms per stroke event at 4K canvas).
- Compositing FPS for a 10-layer, 4000x3000 document (target: 30+ FPS).
- PSD open time for a 500 MB file (target: < 5 seconds).
- Memory usage for a 100-megapixel document (target: < 2 GB resident).

### 8.4 PSD Compatibility Suite

- Automated pipeline: Open PSD in Tuxels, render flat output, compare against Photoshop reference render.
- Per-feature compatibility matrix tracking (blend modes, adjustment types, style effects).
- Community-contributed PSD files with permission for testing.

---

## 9. Risks & Open Questions

### High Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| **PSD format undocumented behaviors** | Files render incorrectly | Build a large test corpus; reverse-engineer Photoshop's behavior case by case |
| **CMYK throughout the pipeline** | Every tool/filter/blend must be CMYK-aware; easy to miss spots | Abstract the channel layout early; test in CMYK mode continuously, not just at the end |
| **Performance at scale** | Large documents feel sluggish | Tile-based architecture, GPU compositing, lazy evaluation, and profiling from day one |
| **Font matching in PSD text layers** | Linux may not have the same fonts as the PSD author | Font substitution with best-match, warn the user, allow manual remapping |
| **Smart Object complexity** | A PSD-within-a-PSD is a recursive document model | Reuse the document model recursively; set a nesting depth limit |

### Open Questions

1. **License model**: GPL v3 (copyleft, ensures derivatives stay open) vs. LGPL / MIT (permissive, allows proprietary plugins)? GPL v3 is recommended for the application, with a permissive API for plugins.
2. **GPU API**: OpenCL (broader hardware support, including AMD and Intel) vs. Vulkan Compute (more modern, harder to write)? Start with OpenCL; port hot paths to Vulkan later if needed.
3. **Wayland vs. X11**: Qt 6 supports both. Primary target is Wayland (the future), with X11 as fallback. Color management on Wayland is still maturing — may need to interface with the compositor directly for accurate color.
4. **Tablet pressure handling**: libinput vs. direct Wacom kernel interface? Qt handles this to some degree; may need custom work for advanced features (tilt, rotation, barrel pressure).
5. **8bf plugin support**: Is it worth pursuing? It would open up thousands of existing Photoshop plugins but requires running Windows DLLs (possibly via Wine integration or a compatibility shim). This is a very late-stage stretch goal.

---

## 10. Success Criteria

### Minimum Viable Product (end of Phase 2)

A designer can:
- [x] Open a multi-layered PSD with groups, masks, and blend modes and see it rendered correctly.
- [x] Make edits using a brush, move tool, and adjustment layers.
- [x] Save back to PSD and share the file with a colleague using Photoshop without data loss.
- [x] Work in 16-bit RGB with an ICC profile.

### Competitive Product (end of Phase 5)

A photographer/designer can:
- [x] Replace their Photoshop workflow for 90% of daily tasks.
- [x] Use Select Subject to mask a person, refine the hair edges, and composite onto a new background.
- [x] Apply layer styles to text for a social media graphic.
- [x] Soft-proof a CMYK print job and export a press-ready file.
- [x] Open Tuxels and feel at home within the first five minutes because the shortcuts and UI layout match what they expect.

### Long-Term Vision

- Recognized as the "Blender of image editors" — a free, open-source tool that professionals actually use in production.
- A healthy contributor community, plugin ecosystem, and sustainable development model.
- Interoperability not just with PSD, but with other open formats (OpenRaster, XCF, AVIF, JPEG XL).

---

*Tuxels is a long journey, but every great open-source project started with a first commit. Let's build something worth switching to.*
