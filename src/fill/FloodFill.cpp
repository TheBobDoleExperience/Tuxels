#include "fill/FloodFill.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "core/SelectionMask.h"

namespace tuxels {

FloodFillResult floodFill(TuxImage& target, int seedX, int seedY,
                          Rgba32F fillColor,
                          const FloodFillOptions& opts,
                          const SelectionMask* selection) {
  FloodFillResult result;
  const int W = target.width();
  const int H = target.height();
  if (W <= 0 || H <= 0) return result;
  if (seedX < 0 || seedY < 0 || seedX >= W || seedY >= H) return result;

  // Per-scanline cached readers so the inner match / blend loops pay one
  // hash lookup per tile boundary instead of per pixel.
  TileRowCursor targetCursor(target);
  auto selCursorPtr = selection
                           ? std::make_unique<TileRowCursor>(selection->image())
                           : nullptr;
  auto inSelection = [&](int x, int y) {
    return selCursorPtr == nullptr || selCursorPtr->sampleR(x, y) > 0.f;
  };
  if (!inSelection(seedX, seedY)) return result;

  // Source color is read from the target BEFORE any writes so tolerance
  // checks stay stable for the whole fill (the standard bucket semantics).
  // We snapshot pixel-by-pixel via a visited bitmap rather than cloning the
  // whole image: each pixel is either not-yet-visited (still-original) or
  // visited (already written, never re-checked).
  const Rgba32F seedColor = targetCursor.at(seedX, seedY);
  const float tol = std::max(0.f, opts.tolerance);

  std::vector<uint8_t> visited(static_cast<size_t>(W) * H, 0);
  auto seen = [&](int x, int y) -> uint8_t& {
    return visited[static_cast<size_t>(y) * W + x];
  };

  auto matches = [&](int x, int y) {
    if (x < 0 || y < 0 || x >= W || y >= H) return false;
    if (seen(x, y)) return false;
    if (!inSelection(x, y)) return false;
    const Rgba32F c = targetCursor.at(x, y);
    return channelDist(c, seedColor) <= tol;
  };

  int minX = W, minY = H, maxX = -1, maxY = -1;

  // Iterative scanline flood. Each stack entry is a seed cell; we expand
  // horizontally to find the [xL, xR] run, mark/fill it, then scan the two
  // neighbor rows above/below for new unvisited runs.
  struct Cell { int x, y; };
  std::vector<Cell> stack;
  stack.reserve(64);
  stack.push_back({seedX, seedY});

  while (!stack.empty()) {
    Cell c = stack.back();
    stack.pop_back();
    if (!matches(c.x, c.y)) continue;

    int xL = c.x;
    while (xL - 1 >= 0 && matches(xL - 1, c.y)) --xL;
    int xR = c.x;
    while (xR + 1 < W && matches(xR + 1, c.y)) ++xR;

    // Fill the run and mark visited. We mark before reading the neighbor
    // rows so a neighbor doesn't push cells we're about to cover.
    for (int x = xL; x <= xR; ++x) {
      seen(x, c.y) = 1;
      const float sel = selCursorPtr ? selCursorPtr->sampleR(x, c.y) : 1.f;
      const float a = opts.opacity * fillColor.a * sel;
      if (a <= 0.f) continue;
      const Rgba32F dst = targetCursor.at(x, c.y);
      const float inv = 1.f - a;
      Rgba32F out;
      out.r = a * fillColor.r + inv * dst.r;
      out.g = a * fillColor.g + inv * dst.g;
      out.b = a * fillColor.b + inv * dst.b;
      out.a = a + inv * dst.a;
      target.setPixel(x, c.y, out);
      ++result.pixelsFilled;
      minX = std::min(minX, x);
      maxX = std::max(maxX, x);
      minY = std::min(minY, c.y);
      maxY = std::max(maxY, c.y);
    }

    // Scan neighbor rows. We walk along the run and push at most one seed
    // per contiguous matching sub-run to avoid re-exploring already-pushed
    // cells (the run-compression trick from the classic Heckbert paper).
    for (int dy : {-1, 1}) {
      const int ny = c.y + dy;
      if (ny < 0 || ny >= H) continue;
      bool inRun = false;
      for (int x = xL; x <= xR; ++x) {
        if (matches(x, ny)) {
          if (!inRun) {
            stack.push_back({x, ny});
            inRun = true;
          }
        } else {
          inRun = false;
        }
      }
    }
  }

  if (maxX >= 0) {
    result.bounds = Rect{minX, minY, maxX - minX + 1, maxY - minY + 1};
    result.changed = result.pixelsFilled > 0;
  }
  return result;
}

std::unique_ptr<SelectionMask> floodSelect(const TuxImage& source,
                                           int seedX, int seedY,
                                           float tolerance,
                                           const SelectionMask* clip) {
  const int W = source.width();
  const int H = source.height();
  if (W <= 0 || H <= 0) return nullptr;
  if (seedX < 0 || seedY < 0 || seedX >= W || seedY >= H) return nullptr;

  TileRowCursor srcCursor(source);
  auto clipCursorPtr =
      clip ? std::make_unique<TileRowCursor>(clip->image()) : nullptr;
  auto inClip = [&](int x, int y) {
    return clipCursorPtr == nullptr || clipCursorPtr->sampleR(x, y) > 0.f;
  };
  if (!inClip(seedX, seedY)) return nullptr;

  const Rgba32F seedColor = srcCursor.at(seedX, seedY);
  const float tol = std::max(0.f, tolerance);

  std::vector<uint8_t> visited(static_cast<size_t>(W) * H, 0);
  auto seen = [&](int x, int y) -> uint8_t& {
    return visited[static_cast<size_t>(y) * W + x];
  };
  auto matches = [&](int x, int y) {
    if (x < 0 || y < 0 || x >= W || y >= H) return false;
    if (seen(x, y)) return false;
    if (!inClip(x, y)) return false;
    const Rgba32F c = srcCursor.at(x, y);
    return channelDist(c, seedColor) <= tol;
  };

  auto out = std::make_unique<SelectionMask>(W, H);
  int pixelsSelected = 0;

  struct Cell { int x, y; };
  std::vector<Cell> stack;
  stack.reserve(64);
  stack.push_back({seedX, seedY});

  while (!stack.empty()) {
    Cell c = stack.back();
    stack.pop_back();
    if (!matches(c.x, c.y)) continue;

    int xL = c.x;
    while (xL - 1 >= 0 && matches(xL - 1, c.y)) --xL;
    int xR = c.x;
    while (xR + 1 < W && matches(xR + 1, c.y)) ++xR;

    // Write the run into the selection mask as one fillRect call so
    // SelectionMask's tile-sparse fillRect path handles the bookkeeping.
    out->fillRect(Rect{xL, c.y, xR - xL + 1, 1}, 1.f);
    for (int x = xL; x <= xR; ++x) seen(x, c.y) = 1;
    pixelsSelected += (xR - xL + 1);

    for (int dy : {-1, 1}) {
      const int ny = c.y + dy;
      if (ny < 0 || ny >= H) continue;
      bool inRun = false;
      for (int x = xL; x <= xR; ++x) {
        if (matches(x, ny)) {
          if (!inRun) {
            stack.push_back({x, ny});
            inRun = true;
          }
        } else {
          inRun = false;
        }
      }
    }
  }

  if (pixelsSelected == 0) return nullptr;
  return out;
}

}  // namespace tuxels
