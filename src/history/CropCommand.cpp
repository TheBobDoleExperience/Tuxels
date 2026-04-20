#include "history/CropCommand.h"

#include <algorithm>
#include <utility>

#include "core/Document.h"
#include "layers/PixelLayer.h"

namespace tuxels {

namespace {

// Deep clone: iterate all tiles, clone each, install into a fresh TuxImage
// of the same dimensions. Tiles outside the source's bounds-tile rect are
// ignored (they shouldn't exist, but be defensive).
TuxImage cloneImage(const TuxImage& src) {
  TuxImage dst(src.width(), src.height());
  for (const auto& [tc, tile] : src.tiles()) {
    if (!tile) continue;
    dst.tiles().set(tc, tile->clone());
  }
  return dst;
}

// Copy a sub-rect of `src` (in `src`'s own coords) into `dst` starting at
// `dst`'s (0, 0). Skips transparent source pixels so the destination stays
// tile-sparse wherever the source was empty. `srcRect` is clamped to `src`'s
// bounds by the caller.
void copySubRectInto(const TuxImage& src, Rect srcRect, TuxImage& dst) {
  for (int y = 0; y < srcRect.h; ++y) {
    for (int x = 0; x < srcRect.w; ++x) {
      Rgba32F p = src.getPixel(srcRect.x + x, srcRect.y + y);
      if (p.a <= 0.f) continue;
      dst.setPixel(x, y, p);
    }
  }
}

// Intersect the layer's content rect (in doc coords) with the crop rect
// (also in doc coords). Returns the overlap in doc coords; when empty the
// returned rect has w == 0 || h == 0.
Rect docOverlap(int originX, int originY, int layerW, int layerH,
                Rect crop) {
  const int x0 = std::max(originX, crop.x);
  const int y0 = std::max(originY, crop.y);
  const int x1 = std::min(originX + layerW, crop.right());
  const int y1 = std::min(originY + layerH, crop.bottom());
  return {x0, y0, std::max(0, x1 - x0), std::max(0, y1 - y0)};
}

std::unique_ptr<SelectionMask> croppedSelection(const SelectionMask& sel,
                                                Rect crop) {
  auto out = std::make_unique<SelectionMask>(crop.w, crop.h);
  const TuxImage& src = sel.image();
  const int x0 = std::max(0, crop.x);
  const int y0 = std::max(0, crop.y);
  const int x1 = std::min(src.width(), crop.right());
  const int y1 = std::min(src.height(), crop.bottom());
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      float r = src.getPixel(x, y).r;
      if (r <= 0.f) continue;
      out->image().setPixel(x - crop.x, y - crop.y,
                            Rgba32F(r, 0.f, 0.f, 1.f));
    }
  }
  if (out->isEmpty()) return nullptr;
  return out;
}

}  // namespace

CropCommand::CropCommand(Document* doc, Rect cropRect)
    : doc_(doc), cropRect_(cropRect) {
  before_ = captureDeep(*doc_);
  applyCropInPlace(*doc_, cropRect_);
}

void CropCommand::undo() {
  if (!doc_) return;
  installDeep(*doc_, before_);
}

void CropCommand::redo() {
  if (!doc_) return;
  applyCropInPlace(*doc_, cropRect_);
}

CropCommand::Snapshot CropCommand::captureDeep(const Document& doc) {
  Snapshot s;
  s.width = doc.width();
  s.height = doc.height();
  const auto& tree = doc.tree();
  s.layers.reserve(tree.size());
  for (std::size_t i = 0; i < tree.size(); ++i) {
    const LayerBase* base = tree.at(i);
    LayerEntry e;
    e.id = base->id;
    e.originX = base->originX;
    e.originY = base->originY;
    if (auto* px = dynamic_cast<const PixelLayer*>(base)) {
      e.image = cloneImage(px->image);
    }
    if (base->mask) {
      e.hasMask = true;
      e.maskImage = cloneImage(base->mask->image);
      e.maskEnabled = base->mask->enabled;
    }
    s.layers.push_back(std::move(e));
  }
  if (doc.selection()) s.selection = doc.selection()->clone();
  return s;
}

void CropCommand::installDeep(Document& doc, const Snapshot& snap) {
  doc.setSize(snap.width, snap.height);
  auto& tree = doc.tree();
  const std::size_t n = std::min(tree.size(), snap.layers.size());
  for (std::size_t i = 0; i < n; ++i) {
    LayerBase* base = tree.at(i);
    const LayerEntry& e = snap.layers[i];
    base->originX = e.originX;
    base->originY = e.originY;
    if (auto* px = dynamic_cast<PixelLayer*>(base)) {
      px->image = cloneImage(e.image);
    }
    if (e.hasMask) {
      if (!base->mask) {
        base->mask = std::make_unique<LayerMask>(e.maskImage.width(),
                                                 e.maskImage.height());
      }
      base->mask->image = cloneImage(e.maskImage);
      base->mask->enabled = e.maskEnabled;
    } else if (base->mask) {
      base->mask.reset();
    }
  }
  doc.setSelection(snap.selection ? snap.selection->clone() : nullptr);
}

void CropCommand::applyCropInPlace(Document& doc, Rect cropRect) {
  auto& tree = doc.tree();
  for (std::size_t i = 0; i < tree.size(); ++i) {
    LayerBase* base = tree.at(i);
    auto* px = dynamic_cast<PixelLayer*>(base);
    if (!px) continue;

    const int origOx = base->originX;
    const int origOy = base->originY;
    const int layerW = px->image.width();
    const int layerH = px->image.height();
    const Rect overlapDoc = docOverlap(origOx, origOy, layerW, layerH,
                                       cropRect);

    if (overlapDoc.w <= 0 || overlapDoc.h <= 0) {
      // No pixels survive. Drop to an empty layer at origin (0, 0) — the
      // layer slot stays in the tree so indexes remain stable for redo and
      // for code that tracks the active layer.
      px->image = TuxImage(0, 0);
      base->originX = 0;
      base->originY = 0;
      if (base->mask) base->mask.reset();
      continue;
    }

    // New layer image sized exactly to the surviving pixel rect, sourced
    // from layer-local coords [overlap - origin .. overlap + size - origin).
    const Rect srcInLayer{overlapDoc.x - origOx, overlapDoc.y - origOy,
                          overlapDoc.w, overlapDoc.h};
    TuxImage newImage(overlapDoc.w, overlapDoc.h);
    copySubRectInto(px->image, srcInLayer, newImage);
    px->image = std::move(newImage);

    if (base->mask) {
      auto newMask = std::make_unique<LayerMask>(overlapDoc.w, overlapDoc.h);
      copySubRectInto(base->mask->image, srcInLayer, newMask->image);
      newMask->enabled = base->mask->enabled;
      base->mask = std::move(newMask);
    }

    base->originX = overlapDoc.x - cropRect.x;
    base->originY = overlapDoc.y - cropRect.y;
  }

  if (const SelectionMask* sel = doc.selection()) {
    doc.setSelection(croppedSelection(*sel, cropRect));
  }
  doc.setSize(cropRect.w, cropRect.h);
}

}  // namespace tuxels
