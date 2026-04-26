#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "layers/LayerBase.h"

namespace tuxels {

class GroupLayer;

// Recursive layer tree. The root holds an ordered list of layers (bottom →
// top, index 0 = bottom); GroupLayer nodes own their own ordered child list
// recursively. Pre-M5 the tree was always flat — the existing flat helpers
// (`size()`, `at()`, `move(from,to)`, `insertAt`, `removeAt`) keep their
// behavior and operate on root-level children. The new path-based helpers
// take a `GroupLayer*` parent (nullptr = root) plus a child index for
// in-tree mutations and lookups.
class LayerTree {
 public:
  // Locator describes where a layer lives within the tree: `parent == nullptr`
  // means the layer is at root level; otherwise the layer is `parent`'s child
  // at `index`. Returned by `locate()`.
  struct Locator {
    GroupLayer* parent = nullptr;
    std::size_t index = 0;
  };

  // --- Flat (root-level) API — preserves pre-M5 semantics ---
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

  // --- Recursive API (M5) ---

  // Find a layer anywhere in the tree by id. Returns nullptr if no match.
  LayerBase* findById(LayerId id);
  const LayerBase* findById(LayerId id) const;

  // Find a layer's location: returns the parent group (nullptr for root) and
  // the child index within that parent. Empty optional when not found.
  std::optional<Locator> locate(LayerId id);

  // Depth-first display order: every layer in the tree, including groups.
  // Within each parent, children appear before the parent itself (groups are
  // emitted child-then-self) so writers and bottom-up consumers can rely on
  // child layers being walked before the group node that contains them.
  std::vector<LayerBase*> flatten();
  std::vector<const LayerBase*> flatten() const;

  // Visit every layer recursively (groups and their children both visited).
  // Visitation order matches `flatten()`.
  void forEach(const std::function<void(LayerBase*)>& fn);
  void forEach(const std::function<void(const LayerBase*)>& fn) const;

  // Insert at a path; `parent == nullptr` inserts at root. Index is clamped
  // to [0, parent's child count].
  void insertAtPath(GroupLayer* parent, std::size_t index,
                    std::unique_ptr<LayerBase> layer);

  // Symmetric removal. Returns nullptr if the path is invalid.
  std::unique_ptr<LayerBase> removeFromPath(GroupLayer* parent,
                                            std::size_t index);

  // Cross-parent move. `fromParent == toParent && fromIdx == toIdx` is a
  // no-op. When source and destination are the same parent and `toIdx >
  // fromIdx`, the destination index is interpreted in the post-removal
  // frame (matches the existing flat `move(from, to)` semantics).
  void move(GroupLayer* fromParent, std::size_t fromIdx,
            GroupLayer* toParent, std::size_t toIdx);

 private:
  std::vector<std::unique_ptr<LayerBase>> layers_;
};

}  // namespace tuxels
