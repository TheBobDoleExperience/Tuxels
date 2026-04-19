#pragma once

#include <memory>
#include <vector>

#include "layers/LayerBase.h"

namespace tuxels {

// Flat ordered list of layers (bottom → top). Index 0 is the bottom. Groups
// are not yet modelled; LayerBase* polymorphism leaves room for them later.
class LayerTree {
 public:
  void add(std::unique_ptr<LayerBase> layer) {
    layers_.push_back(std::move(layer));
  }

  void insertAt(std::size_t index, std::unique_ptr<LayerBase> layer) {
    if (index > layers_.size()) index = layers_.size();
    layers_.insert(layers_.begin() + index, std::move(layer));
  }

  std::unique_ptr<LayerBase> removeAt(std::size_t index) {
    if (index >= layers_.size()) return nullptr;
    auto out = std::move(layers_[index]);
    layers_.erase(layers_.begin() + index);
    return out;
  }

  void move(std::size_t from, std::size_t to) {
    if (from >= layers_.size() || to >= layers_.size() || from == to) return;
    auto tmp = std::move(layers_[from]);
    layers_.erase(layers_.begin() + from);
    if (to > layers_.size()) to = layers_.size();
    layers_.insert(layers_.begin() + to, std::move(tmp));
  }

  std::size_t size() const noexcept { return layers_.size(); }
  bool empty() const noexcept { return layers_.empty(); }

  LayerBase* at(std::size_t i) { return layers_.at(i).get(); }
  const LayerBase* at(std::size_t i) const { return layers_.at(i).get(); }

  const std::vector<std::unique_ptr<LayerBase>>& raw() const { return layers_; }

 private:
  std::vector<std::unique_ptr<LayerBase>> layers_;
};

}  // namespace tuxels
