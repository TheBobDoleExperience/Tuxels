#pragma once

#include <memory>
#include <optional>
#include <string>

namespace tuxels {

class Document;

// Tuxels native file format (v3). Uncompressed little-endian binary that
// round-trips everything the editor model currently owns: document
// dimensions, layer tree (with id / name / visibility / opacity / blend
// mode / origin / dimensions / paint-target / active-layer / non-destructive
// adjustment params), each PixelLayer's tile-sparse TuxImage, each attached
// LayerMask, and the current SelectionMask.
//
// v2 (M2-S0) added per-layer LayerWidth, LayerHeight, OriginX, OriginY so
// placed / moved / transformed layers survive save-load.
// v3 (M3-S4) dispatches on a per-layer `Kind` byte so non-destructive
// adjustment layers (Levels, Curves, ...) round-trip alongside pixel layers.
// v1 and v2 files remain readable — v1 loads with origin=(0,0) and layer
// dims = doc dims; v2 loads as-is (all layers are pixel-kind by construction).
//
// Format layout (header + repeated layer chunks + optional selection chunk):
//
//   Magic         : char[8]  = "TUXELS\\x01\\x00"
//   Version       : uint32   = 3
//   Flags         : uint32   = 0  (reserved; readers must error on unknown)
//   DocWidth      : uint32
//   DocHeight     : uint32
//   ActiveLayer   : int32
//   PaintTarget   : uint8    (0 = Layer, 1 = Mask)
//   HasSelection  : uint8
//   Reserved      : uint16   = 0
//   NumLayers     : uint32
//
//   For each layer (NumLayers times):
//     LayerId       : uint64
//     Kind          : uint8
//                       1 = PixelLayer
//                       2 = LevelsAdjustment      (v3+)
//                       3 = CurvesAdjustment      (v3+)
//                       4 = HueSaturation         (reserved for S6)
//                       5 = BrightnessContrast    (reserved for S6)
//     Visible       : uint8
//     MaskEnabled   : uint8
//     HasMask       : uint8
//     Opacity       : float32
//     BlendMode     : uint32   (BlendMode enum ordinal)
//     LayerWidth    : uint32   (v2+; absent in v1 — defaults to DocWidth.
//                               Adjustment kinds write 0 here since they
//                               carry no backing image.)
//     LayerHeight   : uint32   (v2+; absent in v1 — defaults to DocHeight)
//     OriginX       : int32    (v2+; absent in v1 — defaults to 0.
//                               Adjustment kinds write 0.)
//     OriginY       : int32    (v2+; absent in v1 — defaults to 0)
//     NameLen       : uint32
//     Name          : utf-8 bytes [NameLen]
//     NumImageTiles : uint32   (always 0 for adjustment kinds — kept for
//                               shape uniformity with the pixel branch)
//       For each image tile: TileRecord
//     If Kind == LevelsAdjustment:
//       LevelsDescriptor : { float32 inBlack, inWhite, gamma, outBlack,
//                            outWhite } × 4 channels (Composite/R/G/B)
//                          = 80 bytes.
//     If Kind == CurvesAdjustment:
//       CurvesDescriptor : per channel (Composite/R/G/B):
//                            NumPoints : uint32
//                            Points    : { float32 x, float32 y }[NumPoints]
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
// Adjustment-layer masks are doc-sized (the factory in Document auto-attaches
// a doc-sized white mask on create), so the mask reader constructs at
// `(DocWidth, DocHeight)` when Kind is an adjustment.
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
