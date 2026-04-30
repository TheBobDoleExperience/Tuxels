#pragma once

#include <cstdint>
#include <string_view>

namespace tuxels {

// PS-style per-layer color tag. Used purely as a visual organizer in the
// LayersPanel — affects neither compose nor any pixel data. M8-S0.
enum class LayerColorLabel : uint8_t {
  None = 0,
  Red = 1,
  Orange = 2,
  Yellow = 3,
  Green = 4,
  Blue = 5,
  Violet = 6,
  Gray = 7,
};

inline constexpr int kLayerColorLabelCount = 8;

inline std::string_view layerColorLabelName(LayerColorLabel l) {
  switch (l) {
    case LayerColorLabel::None: return "None";
    case LayerColorLabel::Red: return "Red";
    case LayerColorLabel::Orange: return "Orange";
    case LayerColorLabel::Yellow: return "Yellow";
    case LayerColorLabel::Green: return "Green";
    case LayerColorLabel::Blue: return "Blue";
    case LayerColorLabel::Violet: return "Violet";
    case LayerColorLabel::Gray: return "Gray";
  }
  return "Unknown";
}

}  // namespace tuxels
