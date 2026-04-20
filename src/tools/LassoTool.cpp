#include "tools/LassoTool.h"

#include <algorithm>
#include <cmath>

#include "core/Document.h"

namespace tuxels {

namespace {

// Appends `p` to `buf` iff it's at least `minChordPx` away from the last
// point. Keeps the live polyline compact on slow drags without smoothing
// intentional fine detail.
bool appendDecimated(std::vector<Point2f>& buf, Point2f p, float minChordPx) {
  if (buf.empty()) {
    buf.push_back(p);
    return true;
  }
  const Point2f& last = buf.back();
  const float dx = p.x - last.x;
  const float dy = p.y - last.y;
  if (dx * dx + dy * dy < minChordPx * minChordPx) return false;
  buf.push_back(p);
  return true;
}

}  // namespace

SelectionMode LassoTool::modeFromModifiers(int mods) {
  const bool shift = (mods & Mod::Shift) != 0;
  const bool alt = (mods & Mod::Alt) != 0;
  if (shift && alt) return SelectionMode::Intersect;
  if (shift) return SelectionMode::Add;
  if (alt) return SelectionMode::Subtract;
  return SelectionMode::Replace;
}

void LassoTool::press(Document& doc, float x, float y, MouseButton btn) {
  if (btn != MouseButton::Left) return;
  dragging_ = true;
  points_.clear();
  points_.push_back({x, y});
  pressMods_ = modifiers_;
  docW_ = doc.width();
  docH_ = doc.height();
}

void LassoTool::move(Document& /*doc*/, float x, float y) {
  if (!dragging_) return;
  appendDecimated(points_, {x, y}, 1.f);
}

void LassoTool::release(Document& doc, float x, float y, MouseButton btn) {
  if (btn != MouseButton::Left || !dragging_) return;
  appendDecimated(points_, {x, y}, 0.f);  // always include final cursor
  dragging_ = false;

  // Same modifier-vs-persistent-mode rule as MarqueeTool: mods at press
  // time win if any were held; otherwise fall through to the options-row
  // persistent mode.
  const bool modsHeld = (pressMods_ & (Mod::Shift | Mod::Alt)) != 0;
  const SelectionMode mode =
      modsHeld ? modeFromModifiers(pressMods_) : persistentMode_;

  // A degenerate stroke (two-or-fewer distinct points) is treated as an
  // empty click. Replace-mode empty click collapses to null (Deselect);
  // other modes leave the existing selection alone.
  auto before = doc.selection() ? doc.selection()->clone() : nullptr;
  if (points_.size() < 3) {
    points_.clear();
    if (mode == SelectionMode::Replace && before) {
      pending_ = PendingCommit{std::move(before), nullptr, "Deselect"};
    }
    return;
  }

  auto lasso = std::make_unique<SelectionMask>(docW_, docH_);
  lasso->fillPolygon(points_, 1.f);
  points_.clear();

  std::unique_ptr<SelectionMask> after;
  if (mode == SelectionMode::Replace) {
    after = std::move(lasso);
  } else {
    auto base = doc.selection()
                    ? doc.selection()->clone()
                    : std::make_unique<SelectionMask>(docW_, docH_);
    base->combine(*lasso, mode);
    after = std::move(base);
  }

  // All-zero → null (matches the discipline documented on SelectionMask:
  // null == "no selection, paint everywhere", distinct from an all-zero
  // mask).
  if (after && after->isEmpty()) after.reset();

  const char* label = "Lasso Selection";
  switch (mode) {
    case SelectionMode::Replace:   label = "Lasso Selection"; break;
    case SelectionMode::Add:       label = "Add to Selection"; break;
    case SelectionMode::Subtract:  label = "Subtract from Selection"; break;
    case SelectionMode::Intersect: label = "Intersect Selection"; break;
  }

  pending_ = PendingCommit{std::move(before), std::move(after), label};
}

}  // namespace tuxels
