#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "compositor/BlendMode.h"
#include "core/Pixel.h"
#include "core/Tile.h"
#include "layers/LayerMask.h"

namespace tuxels {

using LayerId = uint64_t;

class LayerBase {
 public:
  virtual ~LayerBase() = default;

  LayerId id = 0;
  std::string name;
  bool visible = true;
  float opacity = 1.f;
  BlendMode blend = BlendMode::Normal;
  std::unique_ptr<LayerMask> mask;

  // Render this layer's contribution for one tile. Writes kTilePx*kTilePx
  // Rgba32F values into `out`. Returns true if any non-transparent pixel was
  // written; false means the tile is empty and the compositor can skip it.
  virtual bool renderTile(TileCoord tc, Rgba32F* out) const = 0;

  // Dissolve noise seed — derived from layer id so patterns are stable.
  virtual uint32_t noiseSeed() const {
    return static_cast<uint32_t>(id * 0x9E3779B97F4A7C15ull);
  }
};

}  // namespace tuxels
