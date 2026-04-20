#pragma once

#include "core/Pixel.h"
#include "core/TuxImage.h"

namespace tuxels {

class SelectionMask;

struct FloodFillOptions {
  // Max L∞ distance (per-channel max |a - b|) a pixel's source color may
  // differ from the seed color and still be flooded. 0 = exact match. Range
  // [0, 1] on normalized float color; the UI maps a 0–255 slider by /255.
  float tolerance = 0.f;

  // Blend factor on the fill color when compositing over the target. 1 =
  // solid replace (modulo selection & color alpha); < 1 = translucent fill.
  float opacity = 1.f;

  // v1 supports only contiguous (4-connected scanline flood). Retained as a
  // field so the UI option has a stable home; non-contiguous fills are
  // deferred to a later step.
  bool contiguous = true;
};

struct FloodFillResult {
  Rect bounds;       // tight pixel bbox of pixels actually written; empty → no-op
  bool changed = false;
  int pixelsFilled = 0;
};

// 4-connected scanline flood from (seedX, seedY). Compares candidate
// pixels' colors against the seed's *source* color — so the fill itself
// doesn't invalidate further tolerance tests mid-fill. The fill is
// clipped to the image bounds and, if `selection` is non-null, to pixels
// where selection > 0 (binary threshold for traversal; the fill alpha is
// soft-multiplied by the selection sample for feathered edges).
//
// Writes are blended src-over using the same straight-alpha math as
// BrushEngine, so bucket and brush produce visually consistent output on
// the same layer. For mask targets the fill color should be set by the
// caller to the desired R value (G/B ignored by mask sampling).
FloodFillResult floodFill(TuxImage& target, int seedX, int seedY,
                          Rgba32F fillColor,
                          const FloodFillOptions& opts,
                          const SelectionMask* selection);

}  // namespace tuxels
