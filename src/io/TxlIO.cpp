#include "io/TxlIO.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <type_traits>
#include <vector>

#include "compositor/BlendMode.h"
#include "core/Document.h"
#include "core/Pixel.h"
#include "core/SelectionMask.h"
#include "core/Tile.h"
#include "core/TileStore.h"
#include "core/TuxImage.h"
#include "geom/Spline.h"
#include "layers/CurvesAdjustment.h"
#include "layers/LayerBase.h"
#include "layers/LayerMask.h"
#include "layers/LevelsAdjustment.h"
#include "layers/PixelLayer.h"

namespace tuxels {

namespace {

constexpr char kMagic[8] = {'T', 'U', 'X', 'E', 'L', 'S', '\x01', '\x00'};
constexpr uint32_t kVersionCurrent = 3;
constexpr uint32_t kVersionV2 = 2;
constexpr uint32_t kVersionV1 = 1;

constexpr uint8_t kLayerKindPixel = 1;
constexpr uint8_t kLayerKindLevels = 2;
constexpr uint8_t kLayerKindCurves = 3;
// Reserved for S6; unused on disk until then.
constexpr uint8_t kLayerKindHueSaturation = 4;
constexpr uint8_t kLayerKindBrightnessContrast = 5;

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

uint8_t kindByte(const LayerBase& layer) {
  if (layer.kind() == LayerKind::Pixel) return kLayerKindPixel;
  if (dynamic_cast<const LevelsAdjustment*>(&layer)) return kLayerKindLevels;
  if (dynamic_cast<const CurvesAdjustment*>(&layer)) return kLayerKindCurves;
  // Unknown adjustment subclass — caller treats 0 as an error sentinel.
  return 0;
}

bool writeLevelsDescriptor(std::ofstream& out,
                            const LevelsAdjustment& layer) {
  const auto& all = layer.allParams();
  for (const auto& p : all) {
    if (!writeRaw(out, p.inBlack)) return false;
    if (!writeRaw(out, p.inWhite)) return false;
    if (!writeRaw(out, p.gamma)) return false;
    if (!writeRaw(out, p.outBlack)) return false;
    if (!writeRaw(out, p.outWhite)) return false;
  }
  return out.good();
}

bool readLevelsDescriptor(std::ifstream& in, LevelsAdjustment& layer) {
  std::array<LevelsParams, 4> all{};
  for (auto& p : all) {
    if (!readRaw(in, p.inBlack)) return false;
    if (!readRaw(in, p.inWhite)) return false;
    if (!readRaw(in, p.gamma)) return false;
    if (!readRaw(in, p.outBlack)) return false;
    if (!readRaw(in, p.outWhite)) return false;
  }
  layer.setAllParams(all);
  return true;
}

bool writeCurvesDescriptor(std::ofstream& out,
                            const CurvesAdjustment& layer) {
  const auto& all = layer.allPoints();
  for (const auto& channel : all) {
    const uint32_t n = static_cast<uint32_t>(channel.size());
    if (!writeRaw(out, n)) return false;
    for (const SplinePoint& p : channel) {
      if (!writeRaw(out, p.x)) return false;
      if (!writeRaw(out, p.y)) return false;
    }
  }
  return out.good();
}

// Sanity cap on per-channel control point count to guard against corrupt
// headers requesting huge allocations. Photoshop UIs cap well below this.
constexpr uint32_t kMaxCurvePointsPerChannel = 1u << 16;

bool readCurvesDescriptor(std::ifstream& in, CurvesAdjustment& layer) {
  CurvesAdjustment::PointsArray all{};
  for (auto& channel : all) {
    uint32_t n = 0;
    if (!readRaw(in, n)) return false;
    if (n > kMaxCurvePointsPerChannel) return false;
    channel.resize(n);
    for (uint32_t i = 0; i < n; ++i) {
      if (!readRaw(in, channel[i].x)) return false;
      if (!readRaw(in, channel[i].y)) return false;
    }
  }
  layer.setAllPoints(all);
  return true;
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
    const uint8_t kind = kindByte(*layer);
    if (kind == 0) {
      setErr(err, "Unsupported layer subclass at layer index " +
                      std::to_string(i));
      return false;
    }

    writeRaw(out, static_cast<uint64_t>(layer->id));
    writeRaw(out, kind);
    writeRaw(out, static_cast<uint8_t>(layer->visible ? 1 : 0));
    writeRaw(out, static_cast<uint8_t>(
                      (layer->mask && layer->mask->enabled) ? 1 : 0));
    writeRaw(out, static_cast<uint8_t>(layer->mask ? 1 : 0));
    writeRaw(out, layer->opacity);
    writeRaw(out, static_cast<uint32_t>(layer->blend));

    // Adjustment layers carry no backing image — their dims / origin are
    // sentinel zeros on disk; masks are doc-sized (reconstructed on load).
    if (kind == kLayerKindPixel) {
      const auto* px = static_cast<const PixelLayer*>(layer);
      writeRaw(out, static_cast<uint32_t>(px->image.width()));
      writeRaw(out, static_cast<uint32_t>(px->image.height()));
      writeRaw(out, static_cast<int32_t>(layer->originX));
      writeRaw(out, static_cast<int32_t>(layer->originY));
    } else {
      writeRaw(out, static_cast<uint32_t>(0));
      writeRaw(out, static_cast<uint32_t>(0));
      writeRaw(out, static_cast<int32_t>(0));
      writeRaw(out, static_cast<int32_t>(0));
    }

    const uint32_t nameLen = static_cast<uint32_t>(layer->name.size());
    writeRaw(out, nameLen);
    if (nameLen > 0) out.write(layer->name.data(), nameLen);

    if (kind == kLayerKindPixel) {
      const auto* px = static_cast<const PixelLayer*>(layer);
      if (!writeImage(out, px->image)) {
        setErr(err, "Failed writing layer image tiles");
        return false;
      }
    } else {
      // Adjustment kinds: NumImageTiles = 0 sentinel for shape uniformity,
      // followed by the kind-specific descriptor.
      writeRaw(out, static_cast<uint32_t>(0));
      if (kind == kLayerKindLevels) {
        const auto* lv = dynamic_cast<const LevelsAdjustment*>(layer);
        if (!lv || !writeLevelsDescriptor(out, *lv)) {
          setErr(err, "Failed writing Levels descriptor");
          return false;
        }
      } else if (kind == kLayerKindCurves) {
        const auto* cv = dynamic_cast<const CurvesAdjustment*>(layer);
        if (!cv || !writeCurvesDescriptor(out, *cv)) {
          setErr(err, "Failed writing Curves descriptor");
          return false;
        }
      } else {
        setErr(err, "Writer does not know how to serialize kind " +
                        std::to_string(kind));
        return false;
      }
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
  if (version != kVersionCurrent && version != kVersionV2 &&
      version != kVersionV1) {
    setErr(err, "Unsupported .txl version: " + std::to_string(version));
    return std::nullopt;
  }
  readRaw(in, flags);
  if (flags != 0) {
    setErr(err, "Unknown .txl flags bits: " + std::to_string(flags));
    return std::nullopt;
  }
  const bool hasOriginFields = (version >= kVersionV2);
  const bool acceptsAdjustmentKinds = (version >= kVersionCurrent);

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
    const bool isAdjustmentKind =
        (kind == kLayerKindLevels || kind == kLayerKindCurves ||
         kind == kLayerKindHueSaturation ||
         kind == kLayerKindBrightnessContrast);
    if (kind != kLayerKindPixel && !isAdjustmentKind) {
      setErr(err, "Unknown layer kind " + std::to_string(kind) +
                      " at index " + std::to_string(li));
      return std::nullopt;
    }
    if (isAdjustmentKind && !acceptsAdjustmentKinds) {
      setErr(err, "Layer kind " + std::to_string(kind) +
                      " requires .txl v3 or later");
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

    std::unique_ptr<LayerBase> layer;
    if (kind == kLayerKindPixel) {
      auto px = std::make_unique<PixelLayer>(static_cast<int>(layerW),
                                              static_cast<int>(layerH));
      if (!readImageInto(in, px->image)) {
        setErr(err, "Failed reading layer image (layer " +
                        std::to_string(li) + ")");
        return std::nullopt;
      }
      layer = std::move(px);
    } else {
      // Adjustment layers: expect NumImageTiles == 0 sentinel, then descriptor.
      uint32_t numImageTiles = 0;
      if (!readRaw(in, numImageTiles)) {
        setErr(err, "Truncated adjustment image-tile count");
        return std::nullopt;
      }
      if (numImageTiles != 0) {
        setErr(err, "Adjustment layer must have 0 image tiles");
        return std::nullopt;
      }
      if (kind == kLayerKindLevels) {
        auto lv = std::make_unique<LevelsAdjustment>();
        if (!readLevelsDescriptor(in, *lv)) {
          setErr(err, "Failed reading Levels descriptor");
          return std::nullopt;
        }
        layer = std::move(lv);
      } else if (kind == kLayerKindCurves) {
        auto cv = std::make_unique<CurvesAdjustment>();
        if (!readCurvesDescriptor(in, *cv)) {
          setErr(err, "Failed reading Curves descriptor");
          return std::nullopt;
        }
        layer = std::move(cv);
      } else {
        // Reserved kinds (HueSat, B&C) are format-valid but no subclass
        // yet exists to materialize them. Fail cleanly.
        setErr(err, "Adjustment kind " + std::to_string(kind) +
                        " is reserved but not yet implemented");
        return std::nullopt;
      }
    }

    layer->id = id;
    layer->name = std::move(name);
    layer->visible = (visible != 0);
    layer->opacity = opacity;
    layer->blend = static_cast<BlendMode>(blend);
    layer->originX = originX;
    layer->originY = originY;

    if (hasMask) {
      // Pixel masks match the backing image; adjustment masks are doc-sized
      // per the `Document::addAdjustmentLayer` invariant.
      const int maskW = (kind == kLayerKindPixel) ? static_cast<int>(layerW)
                                                   : static_cast<int>(w);
      const int maskH = (kind == kLayerKindPixel) ? static_cast<int>(layerH)
                                                   : static_cast<int>(h);
      auto mask = std::make_unique<LayerMask>(maskW, maskH);
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
