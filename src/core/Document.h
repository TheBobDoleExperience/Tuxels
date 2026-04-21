#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>

#include "core/SelectionMask.h"
#include "layers/LayerBase.h"
#include "layers/LayerMask.h"
#include "layers/LayerTree.h"
#include "layers/PixelLayer.h"

namespace tuxels {

enum class PaintTarget { Layer, Mask };

// The editor's top-level model: canvas dimensions + ordered layer tree +
// active-layer index. No Qt dependency so tests and non-UI code can own one.
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

  int activeLayerIndex() const noexcept { return activeLayerIndex_; }
  void setActiveLayerIndex(int i) {
    if (i < -1 || static_cast<std::size_t>(i) >= tree_.size()) {
      activeLayerIndex_ = tree_.empty() ? -1
                                        : static_cast<int>(tree_.size()) - 1;
      return;
    }
    activeLayerIndex_ = i;
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
    if (activeLayerIndex_ < 0) return nullptr;
    return tree_.at(static_cast<std::size_t>(activeLayerIndex_));
  }
  const LayerBase* activeLayer() const {
    if (activeLayerIndex_ < 0) return nullptr;
    return tree_.at(static_cast<std::size_t>(activeLayerIndex_));
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
    tree_.add(std::move(layer));
    activeLayerIndex_ = static_cast<int>(tree_.size()) - 1;
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
    tree_.add(std::move(layer));
    activeLayerIndex_ = static_cast<int>(tree_.size()) - 1;
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
    tree_.add(std::move(layer));
    activeLayerIndex_ = static_cast<int>(tree_.size()) - 1;
    paintTarget_ = PaintTarget::Mask;
    return raw;
  }

 private:
  int width_ = 0;
  int height_ = 0;
  LayerTree tree_;
  int activeLayerIndex_ = -1;
  LayerId nextId_ = 0;
  PaintTarget paintTarget_ = PaintTarget::Layer;
  std::unique_ptr<SelectionMask> selection_;
};

}  // namespace tuxels
