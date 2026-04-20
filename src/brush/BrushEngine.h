#pragma once

#include "brush/RoundBrush.h"
#include "core/TuxImage.h"

namespace tuxels {

class SelectionMask;

// Drives a RoundBrush across a stroke, laying down stamps at spacing
// intervals. Single-responsibility: paints the brush into `target` in
// image-space; it does not know about layers, masks, or UI.
//
// If `selection` is non-null, per-pixel deposit is multiplied by
// `selection->sample(x, y)` so strokes are clipped to the active
// selection. Null selection means "no selection — paint everywhere".
class BrushEngine {
 public:
  BrushEngine(RoundBrush& brush, TuxImage& target,
              const SelectionMask* selection = nullptr)
      : brush_(brush), target_(target), selection_(selection) {}

  // Start a stroke at image-space (x,y). Lays an initial stamp.
  void beginStroke(float x, float y);
  // Continue stroke to (x,y), stamping every spacingPx() pixels along the
  // line from the previous point.
  void continueStroke(float x, float y);
  void endStroke();

  // Lay a single stamp centered at image-space (cx, cy).
  void applyStamp(float cx, float cy);

  bool active() const noexcept { return active_; }
  Rect strokeBounds() const noexcept { return bounds_; }
  void clearBounds() noexcept { bounds_ = {}; }

  // Rect covering stamps laid since the last takeIncrementalBounds() call.
  // Used by the UI to recomposite only the freshly-dirtied region instead
  // of the whole image on every mouse-move tick.
  Rect takeIncrementalBounds() noexcept {
    Rect r = incremental_;
    incremental_ = {};
    return r;
  }

 private:
  void growBounds(int x0, int y0, int x1, int y1);

  RoundBrush& brush_;
  TuxImage& target_;
  const SelectionMask* selection_ = nullptr;
  bool active_ = false;
  float lastX_ = 0.f;
  float lastY_ = 0.f;
  float distAccum_ = 0.f;
  Rect bounds_{};
  Rect incremental_{};
};

}  // namespace tuxels
