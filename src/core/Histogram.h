#pragma once

#include <cstdint>

namespace tuxels {

class TuxImage;
class SelectionMask;

// 4 × 256 bucket histogram — channels 0/1/2 are R/G/B straight, channel 3
// is luma (`0.299*R + 0.587*G + 0.114*B`) so Levels / Curves can paint a
// composite backdrop without re-scanning. `total` is the number of pixels
// that contributed to each channel's bucket sum (fully-transparent pixels
// and selection-clipped pixels are excluded). Values are binned after
// clamping each channel to `[0,1]` and scaling to `[0, 255]` via
// `std::lround` — matches the 256-bucket precision the dialog widgets use
// directly as their X axis.
struct Histogram4x256 {
  uint32_t buckets[4][256]{};
  uint64_t total = 0;
};

// Scan `src` one present tile at a time and bin every in-document,
// non-transparent pixel. When `clip` is non-null each pixel is tested
// against `clip->sample(x,y) >= 0.5` (binary threshold, matching the
// semantics `SelectionMask` uses elsewhere) so the histogram reflects
// only what sits inside the active selection.
//
// Cost is O(N) over the non-empty tiles; allocation-free. Callers that
// want "histogram of the adjustment's input" compose the sub-tree below
// the adjustment into a temp `TuxImage` first, then pass it here.
Histogram4x256 computeHistogram(const TuxImage& src,
                                const SelectionMask* clip = nullptr);

}  // namespace tuxels
