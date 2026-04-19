#pragma once

#include <string>

namespace tuxels {

// Placeholder color-space identifier. M0 is sRGB passthrough; lcms2-based
// transforms land in a later phase. Callers shouldn't branch on this yet.
struct ColorSpace {
  std::string name = "sRGB";

  static ColorSpace sRGB() { return {"sRGB"}; }
};

}  // namespace tuxels
