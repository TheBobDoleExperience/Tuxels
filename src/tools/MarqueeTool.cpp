#include "tools/MarqueeTool.h"

#include <algorithm>
#include <cmath>

#include "core/Document.h"

namespace tuxels {

Rect MarqueeTool::computeRect() const {
  const int x0 = std::min(startX_, curX_);
  const int y0 = std::min(startY_, curY_);
  const int x1 = std::max(startX_, curX_);
  const int y1 = std::max(startY_, curY_);
  return Rect{x0, y0, (x1 - x0) + 1, (y1 - y0) + 1};
}

SelectionMode MarqueeTool::modeFromModifiers(int mods) {
  const bool shift = (mods & Mod::Shift) != 0;
  const bool alt = (mods & Mod::Alt) != 0;
  if (shift && alt) return SelectionMode::Intersect;
  if (shift) return SelectionMode::Add;
  if (alt) return SelectionMode::Subtract;
  return SelectionMode::Replace;
}

void MarqueeTool::press(Document& doc, float x, float y, MouseButton btn) {
  if (btn != MouseButton::Left) return;
  dragging_ = true;
  startX_ = curX_ = static_cast<int>(std::floor(x));
  startY_ = curY_ = static_cast<int>(std::floor(y));
  pressMods_ = modifiers_;
  docW_ = doc.width();
  docH_ = doc.height();
}

void MarqueeTool::move(Document& /*doc*/, float x, float y) {
  if (!dragging_) return;
  curX_ = static_cast<int>(std::floor(x));
  curY_ = static_cast<int>(std::floor(y));
}

void MarqueeTool::release(Document& doc, float x, float y, MouseButton btn) {
  if (btn != MouseButton::Left || !dragging_) return;
  curX_ = static_cast<int>(std::floor(x));
  curY_ = static_cast<int>(std::floor(y));
  dragging_ = false;

  Rect r = computeRect();
  const int x0 = std::max(r.x, 0);
  const int y0 = std::max(r.y, 0);
  const int x1 = std::min(r.right(), docW_);
  const int y1 = std::min(r.bottom(), docH_);
  const bool emptyDrag = (x1 <= x0 || y1 <= y0);

  // Modifiers held at press time win over the persistent mode; this
  // preserves the Photoshop Shift/Alt temporary-override on systems where
  // the WM doesn't grab Alt-drag for window-move. If neither Shift nor Alt
  // was held, fall through to the persistent mode set via the options row.
  const bool modsHeld = (pressMods_ & (Mod::Shift | Mod::Alt)) != 0;
  const SelectionMode mode =
      modsHeld ? modeFromModifiers(pressMods_) : persistentMode_;

  auto before = doc.selection() ? doc.selection()->clone() : nullptr;

  // Short-circuit: an empty drag with Replace means "just click" — collapse
  // to null (deselect). Otherwise it leaves the existing selection alone.
  if (emptyDrag && mode == SelectionMode::Replace) {
    if (!before) return;  // already no selection; nothing to commit
    pending_ = PendingCommit{std::move(before), nullptr, "Deselect"};
    return;
  }
  if (emptyDrag) return;  // no-op add/subtract/intersect with empty rect

  Rect clipped{x0, y0, x1 - x0, y1 - y0};
  auto marquee = std::make_unique<SelectionMask>(docW_, docH_);
  marquee->fillRect(clipped, 1.f);

  std::unique_ptr<SelectionMask> after;
  if (mode == SelectionMode::Replace) {
    after = std::move(marquee);
  } else {
    auto base = doc.selection()
                    ? doc.selection()->clone()
                    : std::make_unique<SelectionMask>(docW_, docH_);
    base->combine(*marquee, mode);
    after = std::move(base);
  }

  // Collapse an all-zero result back to null so the brush paints everywhere
  // (matches our discipline: null == "no selection", not "nothing painted").
  if (after && after->isEmpty()) after.reset();

  const char* label = "Marquee Selection";
  switch (mode) {
    case SelectionMode::Replace:   label = "Marquee Selection"; break;
    case SelectionMode::Add:       label = "Add to Selection"; break;
    case SelectionMode::Subtract:  label = "Subtract from Selection"; break;
    case SelectionMode::Intersect: label = "Intersect Selection"; break;
  }

  pending_ = PendingCommit{std::move(before), std::move(after), label};
}

}  // namespace tuxels
