#include "tools/MagicWandTool.h"

#include <cmath>

#include "core/Document.h"
#include "fill/FloodFill.h"
#include "layers/PixelLayer.h"

namespace tuxels {

SelectionMode MagicWandTool::modeFromModifiers(int mods) {
  const bool shift = (mods & Mod::Shift) != 0;
  const bool alt = (mods & Mod::Alt) != 0;
  if (shift && alt) return SelectionMode::Intersect;
  if (shift) return SelectionMode::Add;
  if (alt) return SelectionMode::Subtract;
  return SelectionMode::Replace;
}

void MagicWandTool::press(Document& doc, float x, float y, MouseButton btn) {
  if (btn != MouseButton::Left) return;
  auto* px = dynamic_cast<PixelLayer*>(doc.activeLayer());
  if (!px) return;

  const int sx = static_cast<int>(std::floor(x));
  const int sy = static_cast<int>(std::floor(y));
  if (!px->image.inBounds(sx, sy)) return;

  const int modsAtPress = modifiers_;
  const bool modsHeld = (modsAtPress & (Mod::Shift | Mod::Alt)) != 0;
  const SelectionMode mode =
      modsHeld ? modeFromModifiers(modsAtPress) : persistentMode_;

  // For Intersect/Subtract within an existing selection, use the current
  // selection as a clip so the wand can never leak pixels outside it. For
  // Replace/Add there's no pre-existing clip — the wand freely explores
  // the whole layer, and combine() handles the rest.
  const SelectionMask* clip =
      (mode == SelectionMode::Intersect || mode == SelectionMode::Subtract)
          ? doc.selection()
          : nullptr;

  auto wandMask = floodSelect(px->image, sx, sy, tolerance_, clip);

  auto before = doc.selection() ? doc.selection()->clone() : nullptr;

  std::unique_ptr<SelectionMask> after;
  if (!wandMask) {
    // Seed yielded no selected pixels (e.g. pure transparent with 0
    // tolerance, or seed outside the clip). Replace collapses to Deselect;
    // other modes are no-ops.
    if (mode == SelectionMode::Replace) {
      if (!before) return;  // nothing was selected, nothing to do
      pending_ = PendingCommit{std::move(before), nullptr, "Deselect"};
    }
    return;
  }

  if (mode == SelectionMode::Replace) {
    after = std::move(wandMask);
  } else {
    auto base = doc.selection()
                    ? doc.selection()->clone()
                    : std::make_unique<SelectionMask>(px->image.width(),
                                                      px->image.height());
    base->combine(*wandMask, mode);
    after = std::move(base);
  }
  if (after && after->isEmpty()) after.reset();

  const char* label = "Magic Wand";
  switch (mode) {
    case SelectionMode::Replace:   label = "Magic Wand"; break;
    case SelectionMode::Add:       label = "Add to Selection"; break;
    case SelectionMode::Subtract:  label = "Subtract from Selection"; break;
    case SelectionMode::Intersect: label = "Intersect Selection"; break;
  }
  pending_ = PendingCommit{std::move(before), std::move(after), label};
}

}  // namespace tuxels
