#include "history/CropCommand.h"

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

// Per-pixel copy from a sub-rect of `src` into the upper-left of `dst`.
// Skips transparent source pixels so the destination stays tile-sparse
// wherever the source was empty. `crop` is clamped to `src`'s bounds.
void copyCropInto(const TuxImage& src, Rect crop, TuxImage& dst) {
  const int x0 = std::max(0, crop.x);
  const int y0 = std::max(0, crop.y);
  const int x1 = std::min(src.width(), crop.right());
  const int y1 = std::min(src.height(), crop.bottom());
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      Rgba32F p = src.getPixel(x, y);
      if (p.a <= 0.f) continue;
      dst.setPixel(x - crop.x, y - crop.y, p);
    }
  }
}

TuxImage croppedImage(const TuxImage& src, Rect crop) {
  TuxImage out(crop.w, crop.h);
  copyCropInto(src, crop, out);
  return out;
}

std::unique_ptr<SelectionMask> croppedSelection(const SelectionMask& sel,
                                                Rect crop) {
  auto out = std::make_unique<SelectionMask>(crop.w, crop.h);
  // SelectionMask stores its bits in the R channel of a TuxImage. The generic
  // copy path (which filters on alpha) would drop non-zero selection with
  // a=0 pixels. Walk R explicitly and write a fully-opaque sentinel where
  // the source is selected.
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
    if (auto* px = dynamic_cast<PixelLayer*>(base)) {
      px->image = cloneImage(e.image);
    }
    if (e.hasMask) {
      if (!base->mask) base->mask = std::make_unique<LayerMask>(snap.width, snap.height);
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
    if (auto* px = dynamic_cast<PixelLayer*>(base)) {
      px->image = croppedImage(px->image, cropRect);
    }
    if (base->mask) {
      // Absent tiles in a mask default to reveal (1.0), so we leave the
      // destination tile-sparse and only copy explicit pixels. Matches the
      // layer-image treatment.
      auto newMask = std::make_unique<LayerMask>(cropRect.w, cropRect.h);
      copyCropInto(base->mask->image, cropRect, newMask->image);
      newMask->enabled = base->mask->enabled;
      base->mask = std::move(newMask);
    }
  }
  if (const SelectionMask* sel = doc.selection()) {
    doc.setSelection(croppedSelection(*sel, cropRect));
  }
  doc.setSize(cropRect.w, cropRect.h);
}

}  // namespace tuxels
