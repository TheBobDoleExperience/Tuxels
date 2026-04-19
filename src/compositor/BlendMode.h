#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace tuxels {

enum class BlendMode : int {
  Normal = 0,
  Dissolve,
  Darken,
  Multiply,
  ColorBurn,
  Lighten,
  Screen,
  ColorDodge,
  Overlay,
  SoftLight,
  HardLight,
  Difference,
  Exclusion,
  // --- not yet implemented (reserved) ---
  // LinearBurn, LinearDodge, VividLight, LinearLight, PinLight, HardMix,
  // Subtract, Divide, Hue, Saturation, Color, Luminosity, DarkerColor, LighterColor,
};

inline constexpr int kImplementedBlendModeCount = 13;

inline std::string_view blendModeName(BlendMode m) {
  switch (m) {
    case BlendMode::Normal:     return "Normal";
    case BlendMode::Dissolve:   return "Dissolve";
    case BlendMode::Darken:     return "Darken";
    case BlendMode::Multiply:   return "Multiply";
    case BlendMode::ColorBurn:  return "Color Burn";
    case BlendMode::Lighten:    return "Lighten";
    case BlendMode::Screen:     return "Screen";
    case BlendMode::ColorDodge: return "Color Dodge";
    case BlendMode::Overlay:    return "Overlay";
    case BlendMode::SoftLight:  return "Soft Light";
    case BlendMode::HardLight:  return "Hard Light";
    case BlendMode::Difference: return "Difference";
    case BlendMode::Exclusion:  return "Exclusion";
  }
  return "Unknown";
}

}  // namespace tuxels
