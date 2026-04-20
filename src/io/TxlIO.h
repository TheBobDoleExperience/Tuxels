#pragma once

#include <memory>
#include <optional>
#include <string>

namespace tuxels {

class Document;

// Tuxels native file format (v1). Uncompressed little-endian binary that
// round-trips everything the editor model currently owns: document
// dimensions, layer tree (with id / name / visibility / opacity / blend
// mode / paint-target / active-layer), each PixelLayer's tile-sparse
// TuxImage, each attached LayerMask, and the current SelectionMask.
//
// Format layout (header + repeated layer chunks + optional selection chunk):
//
//   Magic         : char[8]  = "TUXELS\\x01\\x00"
//   Version       : uint32   = 1
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
//     Kind          : uint8    (1 = PixelLayer; other values reserved)
//     Visible       : uint8
//     MaskEnabled   : uint8
//     HasMask       : uint8
//     Opacity       : float32
//     BlendMode     : uint32   (BlendMode enum ordinal)
//     NameLen       : uint32
//     Name          : utf-8 bytes [NameLen]
//     NumImageTiles : uint32
//       For each image tile: TileRecord
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
// No compression in v1 — float payloads compress well with deflate/zstd but
// the bring-up priority is correctness. `Flags` lets a v2 reader branch on
// a compression bit without breaking the magic or header layout.
bool saveTxl(const std::string& path, const Document& doc,
             std::string* err = nullptr);

// Returns nullopt on error; if `err` is non-null it receives a short
// human-readable reason.
std::optional<std::unique_ptr<Document>> loadTxl(const std::string& path,
                                                 std::string* err = nullptr);

}  // namespace tuxels
