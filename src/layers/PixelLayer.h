#pragma once

#include "core/TuxImage.h"
#include "layers/LayerBase.h"

namespace tuxels {

class PixelLayer : public LayerBase {
 public:
  TuxImage image;

  PixelLayer() = default;
  PixelLayer(int w, int h) : image(w, h) {}

  bool renderTile(TileCoord tc, Rgba32F* out) const override;
};

}  // namespace tuxels
