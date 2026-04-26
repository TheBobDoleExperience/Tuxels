#pragma once

#include <memory>
#include <optional>
#include <string>

namespace tuxels {

class Document;

// Tuxels native file format (v5). Uncompressed little-endian binary that
// round-trips everything the editor model currently owns: document
// dimensions, recursive layer tree (with id / name / visibility / opacity /
// blend mode / origin / dimensions / paint-target / active-layer / non-
// destructive adjustment params / clip-to-below flag / group nesting),
// each PixelLayer's tile-sparse TuxImage, each attached LayerMask, and the
// current SelectionMask.
//
// v2 (M2-S0) added per-layer LayerWidth, LayerHeight, OriginX, OriginY so
// placed / moved / transformed layers survive save-load.
// v3 (M3-S4) dispatches on a per-layer `Kind` byte so non-destructive
// adjustment layers (Levels, Curves, ...) round-trip alongside pixel layers.
// v4 (M4-S1) appends a `ClipToBelow` byte to each layer record so PS-style
// "Create Clipping Mask" survives save-load.
// v5 (M5-S2) adds Kind ordinals 10 = OpenGroup and 11 = CloseGroup. Layer
// records are still a flat sequence on disk (PSD-style section dividers);
// the reader rebuilds the tree via a parse stack. Group records carry a
// 1-byte `IsExpanded` descriptor; group masks are doc-sized like
// adjustments. **NumLayers becomes "record count on disk", not user-facing
// layer count** — a group contributes 2 records (open + close).
// v1, v2, v3, and v4 files remain readable — v1 loads with origin=(0,0) and
// layer dims = doc dims; v2/v3/v4 load as-is. Pre-v4 records load with
// `clipToBelow == false`. Pre-v5 files have no groups (flat layer list).
//
// Format layout (header + repeated layer chunks + optional selection chunk):
//
//   Magic         : char[8]  = "TUXELS\\x01\\x00"
//   Version       : uint32   = 5
//   Flags         : uint32   = 0  (reserved; readers must error on unknown)
//   DocWidth      : uint32
//   DocHeight     : uint32
//   ActiveLayer   : int32    (legacy flat-index of root layer; v5 also uses
//                              this — group nesting doesn't alter the index
//                              of root-level layers, and the active layer
//                              is restored by `Document::setActiveLayerIndex`
//                              shim which walks tree.raw())
//   PaintTarget   : uint8    (0 = Layer, 1 = Mask)
//   HasSelection  : uint8
//   Reserved      : uint16   = 0
//   NumLayers     : uint32   (record count on disk; v5+ groups contribute 2)
//
//   For each record (NumLayers times):
//     LayerId       : uint64   (0 sentinel for CloseGroup)
//     Kind          : uint8
//                       1  = PixelLayer
//                       2  = LevelsAdjustment      (v3+)
//                       3  = CurvesAdjustment      (v3+)
//                       4  = HueSaturation         (v3+)
//                       5  = BrightnessContrast    (v3+)
//                       10 = OpenGroup             (v5+)
//                       11 = CloseGroup            (v5+, header-only marker)
//     Visible       : uint8
//     MaskEnabled   : uint8
//     HasMask       : uint8
//     ClipToBelow   : uint8    (v4+; absent in v1/v2/v3 — defaults to 0.
//                               Honored for adjustment kinds in M4 and for
//                               group kinds in M5.)
//     Opacity       : float32
//     BlendMode     : uint32   (BlendMode enum ordinal; PassThrough is the
//                               default for groups)
//     LayerWidth    : uint32   (v2+; 0 for adjustment + group kinds)
//     LayerHeight   : uint32   (v2+; 0 for adjustment + group kinds)
//     OriginX       : int32    (v2+; 0 for adjustment + group kinds)
//     OriginY       : int32    (v2+; 0 for adjustment + group kinds)
//     NameLen       : uint32
//     Name          : utf-8 bytes [NameLen]
//
//   --- CloseGroup (kind=11) ends here (no payload after the header). ---
//
//     NumImageTiles : uint32   (0 for non-pixel kinds)
//       For each image tile: TileRecord
//     If Kind == LevelsAdjustment:
//       LevelsDescriptor : { float32 inBlack, inWhite, gamma, outBlack,
//                            outWhite } × 4 channels (Composite/R/G/B)
//                          = 80 bytes.
//     If Kind == CurvesAdjustment:
//       CurvesDescriptor : per channel (Composite/R/G/B):
//                            NumPoints : uint32
//                            Points    : { float32 x, float32 y }[NumPoints]
//     If Kind == HueSaturation:
//       HueSatDescriptor : { float32 hueShift, saturation, lightness } = 12 B
//     If Kind == BrightnessContrast:
//       BCDescriptor     : { float32 brightness, contrast } = 8 B
//     If Kind == OpenGroup:
//       GroupDescriptor  : { uint8 IsExpanded } = 1 B
//     NumMaskTiles  : uint32   (always 0 when HasMask is false)
//       For each mask tile: TileRecord
//
//   If HasSelection:
//     NumSelTiles   : uint32
//       For each selection tile: TileRecord
//
// TileRecord:
//   Tx : int32
//   Ty : int32
//   Data : Rgba32F[kTilePx * kTilePx]   (raw host-order floats; 1 MiB)
//
// Adjustment-layer + group masks are doc-sized (the factory in Document
// auto-attaches a doc-sized white mask on create for adjustments; M5
// follows the same convention for group masks), so the mask reader
// constructs at `(DocWidth, DocHeight)` when Kind is non-pixel.
//
// Tree reconstruction at load: the reader maintains a `groupStack` of
// open `GroupLayer*` parents. OpenGroup records construct + push + attach
// to the current parent (or root if the stack is empty); CloseGroup pops;
// other kinds attach to the current parent. EOF with non-empty stack or
// CloseGroup with empty stack are errors.
//
// No compression — float payloads compress well with deflate/zstd but the
// bring-up priority is correctness. `Flags` lets a future version branch on
// a compression bit without breaking the magic or header layout.
bool saveTxl(const std::string& path, const Document& doc,
             std::string* err = nullptr);

// Returns nullopt on error; if `err` is non-null it receives a short
// human-readable reason.
std::optional<std::unique_ptr<Document>> loadTxl(const std::string& path,
                                                 std::string* err = nullptr);

}  // namespace tuxels
