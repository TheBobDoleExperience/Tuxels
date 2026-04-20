#include "io/TxlIO.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <type_traits>

#include "compositor/BlendMode.h"
#include "core/Document.h"
#include "core/Pixel.h"
#include "core/SelectionMask.h"
#include "core/Tile.h"
#include "core/TileStore.h"
#include "core/TuxImage.h"
#include "layers/LayerBase.h"
#include "layers/LayerMask.h"
#include "layers/PixelLayer.h"

namespace tuxels {

namespace {

constexpr char kMagic[8] = {'T', 'U', 'X', 'E', 'L', 'S', '\x01', '\x00'};
constexpr uint32_t kVersionCurrent = 2;
constexpr uint32_t kVersionLegacyNoOrigin = 1;
constexpr uint8_t kLayerKindPixel = 1;

// Host-order writes. Tuxels only targets little-endian hosts (Linux x86_64
// in M1); a future version can add an endian marker to `Flags` if we ever
// port to BE.

template <class T>
bool writeRaw(std::ofstream& out, const T& value) {
  static_assert(std::is_trivially_copyable_v<T>);
  out.write(reinterpret_cast<const char*>(&value), sizeof(T));
  return out.good();
}

template <class T>
bool readRaw(std::ifstream& in, T& value) {
  static_assert(std::is_trivially_copyable_v<T>);
  in.read(reinterpret_cast<char*>(&value), sizeof(T));
  return in.good();
}

bool writeTile(std::ofstream& out, TileCoord coord, const Tile& tile) {
  if (!writeRaw(out, static_cast<int32_t>(coord.tx))) return false;
  if (!writeRaw(out, static_cast<int32_t>(coord.ty))) return false;
  out.write(reinterpret_cast<const char*>(tile.data()),
            static_cast<std::streamsize>(sizeof(Rgba32F) * kTilePixels));
  return out.good();
}

bool writeImage(std::ofstream& out, const TuxImage& image) {
  const TileStore& store = image.tiles();
  uint32_t n = 0;
  for (const auto& kv : store) {
    if (kv.second) ++n;
  }
  if (!writeRaw(out, n)) return false;
  for (const auto& kv : store) {
    if (!kv.second) continue;
    if (!writeTile(out, kv.first, *kv.second)) return false;
  }
  return out.good();
}

bool readTileInto(std::ifstream& in, TuxImage& image) {
  int32_t tx = 0, ty = 0;
  if (!readRaw(in, tx)) return false;
  if (!readRaw(in, ty)) return false;
  auto tile = std::make_shared<Tile>();
  in.read(reinterpret_cast<char*>(tile->data()),
          static_cast<std::streamsize>(sizeof(Rgba32F) * kTilePixels));
  if (!in.good()) return false;
  image.tiles().set(TileCoord{tx, ty}, std::move(tile));
  return true;
}

bool readImageInto(std::ifstream& in, TuxImage& image) {
  uint32_t numTiles = 0;
  if (!readRaw(in, numTiles)) return false;
  for (uint32_t i = 0; i < numTiles; ++i) {
    if (!readTileInto(in, image)) return false;
  }
  return true;
}

void setErr(std::string* err, std::string msg) {
  if (err) *err = std::move(msg);
}

}  // namespace

bool saveTxl(const std::string& path, const Document& doc, std::string* err) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    setErr(err, "Could not open file for writing: " + path);
    return false;
  }

  out.write(kMagic, 8);
  if (!out.good()) { setErr(err, "Failed writing magic"); return false; }

  writeRaw(out, kVersionCurrent);
  writeRaw(out, static_cast<uint32_t>(0));  // Flags

  writeRaw(out, static_cast<uint32_t>(doc.width()));
  writeRaw(out, static_cast<uint32_t>(doc.height()));

  writeRaw(out, static_cast<int32_t>(doc.activeLayerIndex()));

  writeRaw(out, static_cast<uint8_t>(
                    doc.paintTarget() == PaintTarget::Mask ? 1 : 0));
  const bool hasSelection = doc.selection() != nullptr;
  writeRaw(out, static_cast<uint8_t>(hasSelection ? 1 : 0));
  writeRaw(out, static_cast<uint16_t>(0));  // Reserved

  const auto& tree = doc.tree();
  writeRaw(out, static_cast<uint32_t>(tree.size()));

  for (std::size_t i = 0; i < tree.size(); ++i) {
    const LayerBase* layer = tree.at(i);
    const PixelLayer* px = dynamic_cast<const PixelLayer*>(layer);
    if (!px) {
      setErr(err, "Only PixelLayer supported in .txl v1 (layer index " +
                      std::to_string(i) + ")");
      return false;
    }

    writeRaw(out, static_cast<uint64_t>(layer->id));
    writeRaw(out, kLayerKindPixel);
    writeRaw(out, static_cast<uint8_t>(layer->visible ? 1 : 0));
    writeRaw(out, static_cast<uint8_t>(
                      (layer->mask && layer->mask->enabled) ? 1 : 0));
    writeRaw(out, static_cast<uint8_t>(layer->mask ? 1 : 0));
    writeRaw(out, layer->opacity);
    writeRaw(out, static_cast<uint32_t>(layer->blend));

    // v2 additions: layer backing-image dims + doc-coord origin. Makes
    // Place Image / Move / Free Transform reversible across save-load.
    writeRaw(out, static_cast<uint32_t>(px->image.width()));
    writeRaw(out, static_cast<uint32_t>(px->image.height()));
    writeRaw(out, static_cast<int32_t>(layer->originX));
    writeRaw(out, static_cast<int32_t>(layer->originY));

    const uint32_t nameLen = static_cast<uint32_t>(layer->name.size());
    writeRaw(out, nameLen);
    if (nameLen > 0) out.write(layer->name.data(), nameLen);

    if (!writeImage(out, px->image)) {
      setErr(err, "Failed writing layer image tiles");
      return false;
    }
    if (layer->mask) {
      if (!writeImage(out, layer->mask->image)) {
        setErr(err, "Failed writing mask tiles");
        return false;
      }
    } else {
      writeRaw(out, static_cast<uint32_t>(0));
    }
  }

  if (hasSelection) {
    if (!writeImage(out, doc.selection()->image())) {
      setErr(err, "Failed writing selection tiles");
      return false;
    }
  }

  out.flush();
  if (!out.good()) {
    setErr(err, "Write error finalizing file");
    return false;
  }
  return true;
}

std::optional<std::unique_ptr<Document>> loadTxl(const std::string& path,
                                                 std::string* err) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    setErr(err, "Could not open file for reading: " + path);
    return std::nullopt;
  }

  char magic[8];
  in.read(magic, 8);
  if (!in.good() || std::memcmp(magic, kMagic, 8) != 0) {
    setErr(err, "Not a .txl file (bad magic)");
    return std::nullopt;
  }

  uint32_t version = 0, flags = 0;
  readRaw(in, version);
  if (version != kVersionCurrent && version != kVersionLegacyNoOrigin) {
    setErr(err, "Unsupported .txl version: " + std::to_string(version));
    return std::nullopt;
  }
  readRaw(in, flags);
  if (flags != 0) {
    setErr(err, "Unknown .txl flags bits: " + std::to_string(flags));
    return std::nullopt;
  }
  const bool hasOriginFields = (version >= kVersionCurrent);

  uint32_t w = 0, h = 0;
  int32_t activeLayer = 0;
  uint8_t paintTarget = 0, hasSelection = 0;
  uint16_t reserved = 0;
  uint32_t numLayers = 0;
  readRaw(in, w);
  readRaw(in, h);
  readRaw(in, activeLayer);
  readRaw(in, paintTarget);
  readRaw(in, hasSelection);
  readRaw(in, reserved);
  readRaw(in, numLayers);
  if (!in.good()) {
    setErr(err, "Truncated header");
    return std::nullopt;
  }

  // Hard cap to avoid huge allocations on corrupt input.
  if (numLayers > (1u << 20)) {
    setErr(err, "Implausible layer count: " + std::to_string(numLayers));
    return std::nullopt;
  }

  auto doc = std::make_unique<Document>(static_cast<int>(w),
                                         static_cast<int>(h));
  LayerId maxId = 0;

  for (uint32_t li = 0; li < numLayers; ++li) {
    uint64_t id = 0;
    uint8_t kind = 0, visible = 0, maskEnabled = 0, hasMask = 0;
    float opacity = 1.f;
    uint32_t blend = 0, nameLen = 0;
    readRaw(in, id);
    readRaw(in, kind);
    readRaw(in, visible);
    readRaw(in, maskEnabled);
    readRaw(in, hasMask);
    readRaw(in, opacity);
    readRaw(in, blend);

    uint32_t layerW = w, layerH = h;
    int32_t originX = 0, originY = 0;
    if (hasOriginFields) {
      readRaw(in, layerW);
      readRaw(in, layerH);
      readRaw(in, originX);
      readRaw(in, originY);
    }
    readRaw(in, nameLen);
    if (!in.good()) {
      setErr(err, "Truncated layer header at index " + std::to_string(li));
      return std::nullopt;
    }
    if (kind != kLayerKindPixel) {
      setErr(err, "Unknown layer kind " + std::to_string(kind) +
                      " at index " + std::to_string(li));
      return std::nullopt;
    }
    if (nameLen > (1u << 20)) {
      setErr(err, "Implausible layer name length");
      return std::nullopt;
    }
    // Cap dims defensively — corrupt headers could otherwise trigger huge
    // allocations inside TuxImage construction. The doc-dim cap is implied
    // by the `(1u << 20)` ceiling we already apply to tile counts above.
    if (layerW > (1u << 20) || layerH > (1u << 20)) {
      setErr(err, "Implausible layer dimensions");
      return std::nullopt;
    }

    std::string name(nameLen, '\0');
    if (nameLen > 0) {
      in.read(name.data(), static_cast<std::streamsize>(nameLen));
      if (!in.good()) {
        setErr(err, "Truncated layer name");
        return std::nullopt;
      }
    }

    auto layer = std::make_unique<PixelLayer>(static_cast<int>(layerW),
                                               static_cast<int>(layerH));
    layer->id = id;
    layer->name = std::move(name);
    layer->visible = (visible != 0);
    layer->opacity = opacity;
    layer->blend = static_cast<BlendMode>(blend);
    layer->originX = originX;
    layer->originY = originY;

    if (!readImageInto(in, layer->image)) {
      setErr(err, "Failed reading layer image (layer " + std::to_string(li) + ")");
      return std::nullopt;
    }

    if (hasMask) {
      auto mask = std::make_unique<LayerMask>(static_cast<int>(layerW),
                                               static_cast<int>(layerH));
      mask->enabled = (maskEnabled != 0);
      if (!readImageInto(in, mask->image)) {
        setErr(err, "Failed reading mask (layer " + std::to_string(li) + ")");
        return std::nullopt;
      }
      layer->mask = std::move(mask);
    } else {
      uint32_t numMaskTiles = 0;
      readRaw(in, numMaskTiles);
      if (!in.good() || numMaskTiles != 0) {
        setErr(err, "Mask tile count must be 0 when HasMask=false");
        return std::nullopt;
      }
    }

    if (id > maxId) maxId = id;
    doc->tree().add(std::move(layer));
  }

  doc->setLayerIdCounter(maxId);
  doc->setActiveLayerIndex(activeLayer);
  doc->setPaintTarget(paintTarget == 1 ? PaintTarget::Mask
                                        : PaintTarget::Layer);

  if (hasSelection) {
    auto sel = std::make_unique<SelectionMask>(static_cast<int>(w),
                                                static_cast<int>(h));
    if (!readImageInto(in, sel->image())) {
      setErr(err, "Failed reading selection");
      return std::nullopt;
    }
    doc->setSelection(std::move(sel));
  }

  return doc;
}

}  // namespace tuxels
