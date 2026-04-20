#pragma once

#include <algorithm>
#include <cmath>

#include "core/Pixel.h"
#include "core/TuxImage.h"

namespace tuxels {

class SelectionMask;

// L∞ (channel-max) distance over RGB. Shared by the contiguous flood path
// and the non-contiguous Select By Color tool — both compare candidate
// colors against a seed with the same metric so their tolerance sliders
// mean the same thing.
inline float channelDist(const Rgba32F& a, const Rgba32F& b) {
  const float dr = std::fabs(a.r - b.r);
  const float dg = std::fabs(a.g - b.g);
  const float db = std::fabs(a.b - b.b);
  return std::max(dr, std::max(dg, db));
}

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

// Wand-style selection: runs the same 4-connected scanline flood as
// floodFill, but writes the resulting connected region into a fresh
// SelectionMask (R = 1 inside the flooded area, 0 elsewhere) instead of
// blending a color. `source` is read-only; the seed color is sampled from
// it once. Tolerance uses the same L∞ metric over RGB. If `clip` is
// non-null, traversal is gated to pixels with `clip->sample(x,y) > 0` —
// this is how Shift/Alt/Intersect wand gestures in a pre-existing
// selection keep the wand inside the active region. Returns nullptr if
// the seed is out of bounds or outside the clip.
std::unique_ptr<SelectionMask> floodSelect(const TuxImage& source,
                                           int seedX, int seedY,
                                           float tolerance,
                                           const SelectionMask* clip);

}  // namespace tuxels
