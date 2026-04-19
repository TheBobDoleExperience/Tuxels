#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "layers/LayerBase.h"
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

  LayerBase* activeLayer() {
    if (activeLayerIndex_ < 0) return nullptr;
    return tree_.at(static_cast<std::size_t>(activeLayerIndex_));
  }
  const LayerBase* activeLayer() const {
    if (activeLayerIndex_ < 0) return nullptr;
    return tree_.at(static_cast<std::size_t>(activeLayerIndex_));
  }

  LayerId nextLayerId() noexcept { return ++nextId_; }

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

 private:
  int width_ = 0;
  int height_ = 0;
  LayerTree tree_;
  int activeLayerIndex_ = -1;
  LayerId nextId_ = 0;
  PaintTarget paintTarget_ = PaintTarget::Layer;
};

}  // namespace tuxels
