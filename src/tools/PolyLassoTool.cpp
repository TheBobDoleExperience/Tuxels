#include "tools/PolyLassoTool.h"

#include <algorithm>
#include <cmath>

#include "core/Document.h"

namespace tuxels {

SelectionMode PolyLassoTool::modeFromModifiers(int mods) {
  const bool shift = (mods & Mod::Shift) != 0;
  const bool alt = (mods & Mod::Alt) != 0;
  if (shift && alt) return SelectionMode::Intersect;
  if (shift) return SelectionMode::Add;
  if (alt) return SelectionMode::Subtract;
  return SelectionMode::Replace;
}

void PolyLassoTool::press(Document& doc, float x, float y, MouseButton btn) {
  if (btn != MouseButton::Left) return;
  if (!building_) {
    // First vertex. Capture press-time modifiers so the eventual commit
    // uses the same Shift/Alt state the user held when they started the
    // polygon — a single late modifier tap won't flip the combine mode
    // mid-draw.
    building_ = true;
    points_.clear();
    pressMods_ = modifiers_;
    docW_ = doc.width();
    docH_ = doc.height();
    points_.push_back({x, y});
    cursor_ = {x, y};
    return;
  }
  // Building: a click near the first vertex closes the polygon (with at
  // least 3 distinct points, matching Photoshop's close-on-start gesture).
  if (points_.size() >= 3) {
    const float dx = x - points_.front().x;
    const float dy = y - points_.front().y;
    if (dx * dx + dy * dy <= kCloseRadiusPx * kCloseRadiusPx) {
      cursor_ = {x, y};
      buildCommit(doc);
      return;
    }
  }
  // Otherwise append a new vertex.
  points_.push_back({x, y});
  cursor_ = {x, y};
}

void PolyLassoTool::move(Document& /*doc*/, float x, float y) {
  if (!building_) return;
  cursor_ = {x, y};
}

void PolyLassoTool::release(Document& /*doc*/, float /*x*/, float /*y*/,
                            MouseButton /*btn*/) {
  // No-op. Click-based tool — state transitions live in press/finish.
}

void PolyLassoTool::hover(Document& /*doc*/, float x, float y) {
  if (!building_) return;
  cursor_ = {x, y};
}

void PolyLassoTool::finish(Document& doc) {
  if (!building_) return;
  buildCommit(doc);
}

void PolyLassoTool::cancel() {
  building_ = false;
  points_.clear();
  pending_.reset();
}

void PolyLassoTool::buildCommit(Document& doc) {
  const bool modsHeld = (pressMods_ & (Mod::Shift | Mod::Alt)) != 0;
  const SelectionMode mode =
      modsHeld ? modeFromModifiers(pressMods_) : persistentMode_;

  auto before = doc.selection() ? doc.selection()->clone() : nullptr;
  if (points_.size() < 3) {
    // Degenerate — treat like an empty click: Replace-mode collapses to
    // Deselect when something was selected, otherwise leaves state alone.
    building_ = false;
    points_.clear();
    if (mode == SelectionMode::Replace && before) {
      pending_ = PendingCommit{std::move(before), nullptr, "Deselect"};
    }
    return;
  }

  auto lasso = std::make_unique<SelectionMask>(docW_, docH_);
  lasso->fillPolygon(points_, 1.f);

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
  if (after && after->isEmpty()) after.reset();

  const char* label = "Polygonal Lasso";
  switch (mode) {
    case SelectionMode::Replace:   label = "Polygonal Lasso"; break;
    case SelectionMode::Add:       label = "Add to Selection"; break;
    case SelectionMode::Subtract:  label = "Subtract from Selection"; break;
    case SelectionMode::Intersect: label = "Intersect Selection"; break;
  }

  pending_ = PendingCommit{std::move(before), std::move(after), label};
  building_ = false;
  points_.clear();
}

}  // namespace tuxels
