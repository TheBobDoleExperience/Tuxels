#pragma once

#include <memory>

#include "core/TuxImage.h"

namespace tuxels {

enum class SelectionMode { Replace, Add, Subtract, Intersect };

// A document-sized grayscale mask that tools consult when writing pixels.
// Intensity lives in the underlying TuxImage's R channel (0 = not selected,
// 1 = fully selected; partial values reserved for future feathered / AA
// selections). G, B, A are unused.
//
// A null `Document::selection()` is the canonical "no selection — paint
// everywhere" state. We do not produce an all-zero SelectionMask on
// purpose (Deselect sets the pointer to null instead).
class SelectionMask {
 public:
  SelectionMask(int docW, int docH) : image_(docW, docH) {}

  int width() const noexcept { return image_.width(); }
  int height() const noexcept { return image_.height(); }
  Rect docBounds() const { return image_.bounds(); }

  TuxImage& image() noexcept { return image_; }
  const TuxImage& image() const noexcept { return image_; }

  // Per-pixel sample used in the brush inner loop. Returns 0 outside the
  // document. Cheap but one tile hash per call — callers that sample a row
  // should consider caching; M1 BrushEngine currently does not.
  float sample(int x, int y) const noexcept {
    return image_.getPixel(x, y).r;
  }

  // Fill the intersection of `r` with the document with `value ∈ [0, 1]`.
  void fillRect(Rect r, float value = 1.f);

  // In-place combine `other` onto self per the given mode:
  //   Replace   → out = b
  //   Add       → out = max(a, b)
  //   Subtract  → out = a * (1 - b)
  //   Intersect → out = min(a, b)
  // Both masks must share doc dimensions.
  void combine(const SelectionMask& other, SelectionMode mode);

  // Flip every pixel inside the document (R → 1 - R).
  void invert();

  // True when every pixel in the document is ≤ epsilon. Callers use this to
  // decide whether to collapse an all-zero mask back to a null selection
  // (our canonical "no selection" state).
  bool isEmpty(float epsilon = 1e-5f) const;

  // Tight doc-pixel-space bounding rect of pixels > threshold. Empty rect
  // when nothing is selected. Used by the marching-ants overlay to walk
  // only the interesting area instead of the whole document.
  Rect boundsOfSelected(float threshold = 0.5f) const;

  std::unique_ptr<SelectionMask> clone() const;

  static std::unique_ptr<SelectionMask> makeAll(int w, int h);

 private:
  TuxImage image_;
};

}  // namespace tuxels
