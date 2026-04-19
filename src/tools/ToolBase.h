#pragma once

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
};

}  // namespace tuxels
