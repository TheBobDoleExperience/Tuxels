#pragma once

#include <optional>

#include "core/TuxImage.h"

namespace tuxels {

class Document;

enum class MouseButton { Left, Middle, Right, Other };

// Modifier bitmask passed to tools via `setModifiers()` before press/move/
// release. Kept Qt-free so tuxels_core has no GUI dependency; CanvasView
// translates Qt::KeyboardModifiers into this bitmask at the call site.
namespace Mod {
inline constexpr int None = 0;
inline constexpr int Shift = 1 << 0;
inline constexpr int Alt = 1 << 1;
inline constexpr int Ctrl = 1 << 2;
}  // namespace Mod

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

  // True if the tool wants to receive Shift+Left clicks. When false,
  // CanvasView treats Shift+Left as a pan gesture (existing behavior).
  // Marquee-style tools override to read Shift as an add-to-selection
  // modifier.
  virtual bool consumesShiftClick() const { return false; }

  // Live rubber-band rectangle in document pixel coordinates, during a
  // drag. nullopt when no drag is active or the tool doesn't have one.
  // Rendered by CanvasView as a dashed outline on every mouse event.
  virtual std::optional<Rect> liveRect() const { return std::nullopt; }

  // Called by CanvasView before each press/move/release so the tool can
  // branch on current modifier state (e.g. marquee Add/Subtract/Intersect).
  void setModifiers(int flags) noexcept { modifiers_ = flags; }
  int modifiers() const noexcept { return modifiers_; }

 protected:
  int modifiers_ = 0;
};

}  // namespace tuxels
