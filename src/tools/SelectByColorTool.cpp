#include "tools/SelectByColorTool.h"

#include <cmath>

#include "core/Document.h"
#include "core/Tile.h"
#include "fill/FloodFill.h"
#include "layers/PixelLayer.h"

namespace tuxels {

SelectionMode SelectByColorTool::modeFromModifiers(int mods) {
  const bool shift = (mods & Mod::Shift) != 0;
  const bool alt = (mods & Mod::Alt) != 0;
  if (shift && alt) return SelectionMode::Intersect;
  if (shift) return SelectionMode::Add;
  if (alt) return SelectionMode::Subtract;
  return SelectionMode::Replace;
}

void SelectByColorTool::press(Document& doc, float x, float y,
                              MouseButton btn) {
  if (btn != MouseButton::Left) return;
  auto* px = dynamic_cast<PixelLayer*>(doc.activeLayer());
  if (!px) return;

  // Click is in doc coords; sample the seed at the corresponding layer-local
  // pixel (honours origin from S0). Out-of-layer clicks are no-ops.
  const int seedDocX = static_cast<int>(std::floor(x));
  const int seedDocY = static_cast<int>(std::floor(y));
  const int seedLx = seedDocX - px->originX;
  const int seedLy = seedDocY - px->originY;
  if (!px->image.inBounds(seedLx, seedLy)) return;
  const Rgba32F seed = px->image.getPixel(seedLx, seedLy);

  const int modsAtPress = modifiers_;
  const bool modsHeld = (modsAtPress & (Mod::Shift | Mod::Alt)) != 0;
  const SelectionMode mode =
      modsHeld ? modeFromModifiers(modsAtPress) : persistentMode_;

  // Same clip discipline as the contiguous wand: Intersect/Subtract keep
  // traversal inside the existing selection so the pick can never leak.
  const SelectionMask* clip =
      (mode == SelectionMode::Intersect || mode == SelectionMode::Subtract)
          ? doc.selection()
          : nullptr;

  const int docW = doc.width();
  const int docH = doc.height();
  const int layerW = px->image.width();
  const int layerH = px->image.height();

  auto picked = std::make_unique<SelectionMask>(docW, docH);
  bool anyHit = false;

  // Walk only the present tiles — transparent holes aren't stored and have
  // nothing to match against.
  for (const auto& [tc, tilePtr] : px->image.tiles()) {
    if (!tilePtr) continue;
    const int x0 = tc.tx * kTilePx;
    const int y0 = tc.ty * kTilePx;
    for (int ty = 0; ty < kTilePx; ++ty) {
      const int ly = y0 + ty;
      if (ly >= layerH) break;
      const int dy = ly + px->originY;
      if (dy < 0 || dy >= docH) continue;
      for (int tx = 0; tx < kTilePx; ++tx) {
        const int lx = x0 + tx;
        if (lx >= layerW) break;
        const int dx = lx + px->originX;
        if (dx < 0 || dx >= docW) continue;
        const Rgba32F c = tilePtr->at(tx, ty);
        if (channelDist(c, seed) > tolerance_) continue;
        if (clip && clip->sample(dx, dy) <= 0.f) continue;
        picked->image().setPixel(dx, dy, Rgba32F{1.f, 0.f, 0.f, 1.f});
        anyHit = true;
      }
    }
  }

  auto before = doc.selection() ? doc.selection()->clone() : nullptr;

  if (!anyHit) {
    // Seed matched nothing (e.g. out-of-clip, or some future empty layer).
    // Replace collapses to Deselect when something was selected; else no-op.
    if (mode == SelectionMode::Replace) {
      if (!before) return;
      pending_ = PendingCommit{std::move(before), nullptr, "Deselect"};
    }
    return;
  }

  std::unique_ptr<SelectionMask> after;
  if (mode == SelectionMode::Replace) {
    after = std::move(picked);
  } else {
    auto base = doc.selection()
                    ? doc.selection()->clone()
                    : std::make_unique<SelectionMask>(docW, docH);
    base->combine(*picked, mode);
    after = std::move(base);
  }
  if (after && after->isEmpty()) after.reset();

  const char* label = "Select By Color";
  switch (mode) {
    case SelectionMode::Replace:   label = "Select By Color"; break;
    case SelectionMode::Add:       label = "Add to Selection"; break;
    case SelectionMode::Subtract:  label = "Subtract from Selection"; break;
    case SelectionMode::Intersect: label = "Intersect Selection"; break;
  }
  pending_ = PendingCommit{std::move(before), std::move(after), label};
}

}  // namespace tuxels
