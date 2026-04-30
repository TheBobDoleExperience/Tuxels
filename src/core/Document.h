#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "core/SelectionMask.h"
#include "layers/LayerBase.h"
#include "layers/LayerMask.h"
#include "layers/LayerTree.h"
#include "layers/PixelLayer.h"

namespace tuxels {

enum class PaintTarget { Layer, Mask };

// The editor's top-level model: canvas dimensions + ordered layer tree +
// active-layer id. No Qt dependency so tests and non-UI code can own one.
//
// M5: active-layer state is keyed by `LayerId` (a stable uint64) instead of
// a flat index, so reorders / regroups don't accidentally reseat the active
// layer. The legacy `activeLayerIndex()` / `setActiveLayerIndex(int)` API is
// kept as a transitional shim that walks `tree.raw()` (root-level only) —
// useful while M5 step-by-step migrates call sites; safe to delete once all
// sites are converted.
class Document {
 public:
  Document() = default;
  Document(int width, int height) : width_(width), height_(height) {}

  int width() const noexcept { return width_; }
  int height() const noexcept { return height_; }

  // Used by crop/resize operations. Does not touch layers — callers are
  // responsible for resizing/moving each layer's TuxImage to match.
  void setSize(int w, int h) noexcept {
    width_ = w;
    height_ = h;
  }

  LayerTree& tree() noexcept { return tree_; }
  const LayerTree& tree() const noexcept { return tree_; }

  // M5 canonical active-layer API: id-keyed, survives reorders + regroups.
  LayerId activeLayerId() const noexcept { return activeLayerId_; }
  void setActiveLayerId(LayerId id) noexcept {
    if (id == 0) {
      activeLayerId_ = 0;
      return;
    }
    if (tree_.findById(id) != nullptr) {
      activeLayerId_ = id;
    }
  }

  // Transitional shim — flat-index view of root-level children. Delete once
  // all M5 call sites are converted to id.
  int activeLayerIndex() const noexcept {
    if (activeLayerId_ == 0) return -1;
    for (std::size_t i = 0; i < tree_.size(); ++i) {
      if (tree_.at(i)->id == activeLayerId_) return static_cast<int>(i);
    }
    return -1;
  }
  void setActiveLayerIndex(int i) {
    if (i < -1 || static_cast<std::size_t>(i) >= tree_.size()) {
      // Match legacy clamp behavior: out-of-range falls back to top.
      if (tree_.empty()) {
        activeLayerId_ = 0;
        return;
      }
      activeLayerId_ = tree_.at(tree_.size() - 1)->id;
      return;
    }
    if (i < 0) {
      activeLayerId_ = 0;
      return;
    }
    activeLayerId_ = tree_.at(static_cast<std::size_t>(i))->id;
  }

  PaintTarget paintTarget() const noexcept { return paintTarget_; }
  void setPaintTarget(PaintTarget t) noexcept { paintTarget_ = t; }

  // The active selection, or nullptr when "nothing selected == paint
  // everywhere". Tools consult this when writing pixels; the compositor
  // does not (selection is edit-time, not display-time).
  SelectionMask* selection() noexcept { return selection_.get(); }
  const SelectionMask* selection() const noexcept { return selection_.get(); }
  void setSelection(std::unique_ptr<SelectionMask> sel) {
    selection_ = std::move(sel);
  }

  LayerBase* activeLayer() {
    return activeLayerId_ == 0 ? nullptr : tree_.findById(activeLayerId_);
  }
  const LayerBase* activeLayer() const {
    return activeLayerId_ == 0 ? nullptr : tree_.findById(activeLayerId_);
  }

  // M6-S2 multi-selection. Independent of `activeLayerId_` (the anchor /
  // last-clicked row); the panel maintains both. Tools currently read only
  // `activeLayer()`; batch ops (delete / group / visibility) read this set
  // and operate on the union. Stale ids (layer destroyed) are filtered by
  // `selectedLayers()`.
  const std::vector<LayerId>& selectedLayerIds() const noexcept {
    return selectedLayerIds_;
  }
  void setSelectedLayerIds(std::vector<LayerId> ids) noexcept {
    selectedLayerIds_ = std::move(ids);
  }
  std::vector<LayerBase*> selectedLayers() {
    std::vector<LayerBase*> out;
    out.reserve(selectedLayerIds_.size());
    for (LayerId id : selectedLayerIds_) {
      if (auto* l = tree_.findById(id)) out.push_back(l);
    }
    return out;
  }

  LayerId nextLayerId() noexcept { return ++nextId_; }

  // Seed the internal id counter so the next call to `nextLayerId()` returns
  // `v + 1`. Used by `loadTxl` so ids assigned to layers created after load
  // don't collide with ids stored in the file.
  void setLayerIdCounter(LayerId v) noexcept { nextId_ = v; }

  // Convenience: create a blank pixel layer of the current canvas size,
  // append to the top of the tree, set it active.
  PixelLayer* addBlankPixelLayer(const std::string& name) {
    auto layer = std::make_unique<PixelLayer>(width_, height_);
    layer->id = nextLayerId();
    layer->name = name;
    PixelLayer* raw = layer.get();
    activeLayerId_ = layer->id;
    tree_.add(std::move(layer));
    return raw;
  }

  // Append a pixel layer populated from an existing TuxImage at doc-coord
  // origin `(ox, oy)`. Used by Place Image (S1) so placed PNGs can keep
  // offscreen pixels when they exceed the doc bounds.
  PixelLayer* addPixelLayer(TuxImage&& img, int ox, int oy,
                            const std::string& name) {
    auto layer = std::make_unique<PixelLayer>();
    layer->image = std::move(img);
    layer->id = nextLayerId();
    layer->name = name;
    layer->originX = ox;
    layer->originY = oy;
    PixelLayer* raw = layer.get();
    activeLayerId_ = layer->id;
    tree_.add(std::move(layer));
    return raw;
  }

  // Insert a non-destructive adjustment layer at the top of the tree.
  // Auto-attaches a doc-sized white mask (`enabled = true`) and flips
  // `paintTarget` to `Mask` so the user can immediately paint to restrict
  // where the adjustment applies — matches PS's auto-mask affordance.
  // Caller retains a raw pointer for subclass-specific follow-up (param
  // edits, dialog wiring, etc.).
  template <class T>
  T* addAdjustmentLayer(std::unique_ptr<T> layer) {
    static_assert(std::is_base_of_v<LayerBase, T>,
                  "addAdjustmentLayer: T must derive from LayerBase");
    layer->id = nextLayerId();
    auto mask = std::make_unique<LayerMask>(width_, height_);
    mask->image.fill(Rgba32F{1.f, 1.f, 1.f, 1.f});
    mask->enabled = true;
    layer->mask = std::move(mask);
    T* raw = layer.get();
    activeLayerId_ = layer->id;
    tree_.add(std::move(layer));
    paintTarget_ = PaintTarget::Mask;
    return raw;
  }

 private:
  int width_ = 0;
  int height_ = 0;
  LayerTree tree_;
  LayerId activeLayerId_ = 0;
  LayerId nextId_ = 0;
  PaintTarget paintTarget_ = PaintTarget::Layer;
  std::unique_ptr<SelectionMask> selection_;
  std::vector<LayerId> selectedLayerIds_;
};

}  // namespace tuxels
