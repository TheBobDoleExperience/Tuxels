#pragma once

#include <optional>

#include "core/TuxImage.h"

namespace tuxels {

class Document;

enum class MouseButton { Left, Middle, Right, Other };

// Abstract base for editing tools. CanvasView forwards widget mouse events
// to the active tool, converting coordinates to document image-space first.
class ToolBase {
 public:
  virtual ~ToolBase() = default;
  virtual void press(Document& doc, float x, float y, MouseButton btn) = 0;
  virtual void move(Document& doc, float x, float y) = 0;
  virtual void release(Document& doc, float x, float y, MouseButton btn) = 0;

  // Image-space rect of pixels newly dirtied since the last call. CanvasView
  // reads this after each mouse event to trigger a partial recomposite
  // instead of repainting the whole document. Default: nothing dirty.
  virtual Rect takeDirtyRect() { return {}; }

  // If the tool paints with a finite footprint, return its radius in
  // image-space pixels so the canvas can draw a preview ring under the
  // cursor. std::nullopt → no ring (e.g. Move, Eyedropper).
  virtual std::optional<float> cursorRadiusPx() const { return std::nullopt; }
};

}  // namespace tuxels
