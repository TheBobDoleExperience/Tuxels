#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace tuxels {

class Document;

// Read-only PSD import. M12. Scope:
//  - 8-bit RGB / RGBA color mode (mode == 3, depth == 8). Other depths
//    (1, 16, 32) and modes (Grayscale, CMYK, Lab, Indexed) currently
//    rejected with an explicit error.
//  - Compression 0 (raw) and 1 (PackBits RLE). ZIP variants (2, 3)
//    rejected.
//  - Pixel layers + groups (via 'lsct' section dividers) + per-layer
//    masks. Adjustment layers, smart objects, text layers, layer
//    effects, and clipping flags are not interpreted — adjustment +
//    smart-object + text records flatten to PixelLayers carrying their
//    composite contribution where available; otherwise they're skipped.
//  - PSB (large-document) format is rejected — PSD only.
//
// Returns a freshly-constructed Document on success. On failure returns
// std::nullopt; if `err` is non-null, a human-readable description is
// written there.
std::optional<std::unique_ptr<Document>> loadPsd(const std::string& path,
                                                  std::string* err = nullptr);

// Same, but parses an in-memory byte buffer. Lets unit tests build PSD
// fixtures programmatically without touching the filesystem.
std::optional<std::unique_ptr<Document>> loadPsdBytes(
    std::span<const std::uint8_t> bytes, std::string* err = nullptr);

}  // namespace tuxels
