#include "layers/LayerTree.h"

#include "layers/GroupLayer.h"

namespace tuxels {

namespace {

// Pick the right child vector for a given parent: nullptr → tree root.
std::vector<std::unique_ptr<LayerBase>>& childrenOf(
    std::vector<std::unique_ptr<LayerBase>>& root, GroupLayer* parent) {
  return parent ? parent->children : root;
}

LayerBase* findByIdRec(const std::vector<std::unique_ptr<LayerBase>>& children,
                       LayerId id) {
  for (const auto& up : children) {
    LayerBase* l = up.get();
    if (!l) continue;
    if (l->id == id) return l;
    if (auto* g = dynamic_cast<GroupLayer*>(l)) {
      if (auto* hit = findByIdRec(g->children, id)) return hit;
    }
  }
  return nullptr;
}

bool locateRec(std::vector<std::unique_ptr<LayerBase>>& children,
               GroupLayer* parent, LayerId id, LayerTree::Locator& out) {
  for (std::size_t i = 0; i < children.size(); ++i) {
    LayerBase* l = children[i].get();
    if (!l) continue;
    if (l->id == id) {
      out.parent = parent;
      out.index = i;
      return true;
    }
    if (auto* g = dynamic_cast<GroupLayer*>(l)) {
      if (locateRec(g->children, g, id, out)) return true;
    }
  }
  return false;
}

void flattenRec(std::vector<std::unique_ptr<LayerBase>>& children,
                std::vector<LayerBase*>& out) {
  for (const auto& up : children) {
    LayerBase* l = up.get();
    if (!l) continue;
    if (auto* g = dynamic_cast<GroupLayer*>(l)) {
      flattenRec(g->children, out);
    }
    out.push_back(l);
  }
}

void flattenRecConst(const std::vector<std::unique_ptr<LayerBase>>& children,
                     std::vector<const LayerBase*>& out) {
  for (const auto& up : children) {
    const LayerBase* l = up.get();
    if (!l) continue;
    if (const auto* g = dynamic_cast<const GroupLayer*>(l)) {
      flattenRecConst(g->children, out);
    }
    out.push_back(l);
  }
}

void forEachRec(std::vector<std::unique_ptr<LayerBase>>& children,
                const std::function<void(LayerBase*)>& fn) {
  for (const auto& up : children) {
    LayerBase* l = up.get();
    if (!l) continue;
    if (auto* g = dynamic_cast<GroupLayer*>(l)) {
      forEachRec(g->children, fn);
    }
    fn(l);
  }
}

void forEachRecConst(const std::vector<std::unique_ptr<LayerBase>>& children,
                     const std::function<void(const LayerBase*)>& fn) {
  for (const auto& up : children) {
    const LayerBase* l = up.get();
    if (!l) continue;
    if (const auto* g = dynamic_cast<const GroupLayer*>(l)) {
      forEachRecConst(g->children, fn);
    }
    fn(l);
  }
}

}  // namespace

LayerBase* LayerTree::findById(LayerId id) {
  return findByIdRec(layers_, id);
}

const LayerBase* LayerTree::findById(LayerId id) const {
  return findByIdRec(const_cast<std::vector<std::unique_ptr<LayerBase>>&>(
                         layers_),
                     id);
}

std::optional<LayerTree::Locator> LayerTree::locate(LayerId id) {
  Locator loc;
  if (locateRec(layers_, /*parent=*/nullptr, id, loc)) return loc;
  return std::nullopt;
}

std::vector<LayerBase*> LayerTree::flatten() {
  std::vector<LayerBase*> out;
  flattenRec(layers_, out);
  return out;
}

std::vector<const LayerBase*> LayerTree::flatten() const {
  std::vector<const LayerBase*> out;
  flattenRecConst(layers_, out);
  return out;
}

void LayerTree::forEach(const std::function<void(LayerBase*)>& fn) {
  forEachRec(layers_, fn);
}

void LayerTree::forEach(const std::function<void(const LayerBase*)>& fn) const {
  forEachRecConst(layers_, fn);
}

void LayerTree::insertAtPath(GroupLayer* parent, std::size_t index,
                              std::unique_ptr<LayerBase> layer) {
  auto& vec = childrenOf(layers_, parent);
  if (index > vec.size()) index = vec.size();
  vec.insert(vec.begin() + index, std::move(layer));
}

std::unique_ptr<LayerBase> LayerTree::removeFromPath(GroupLayer* parent,
                                                      std::size_t index) {
  auto& vec = childrenOf(layers_, parent);
  if (index >= vec.size()) return nullptr;
  auto out = std::move(vec[index]);
  vec.erase(vec.begin() + index);
  return out;
}

void LayerTree::move(GroupLayer* fromParent, std::size_t fromIdx,
                      GroupLayer* toParent, std::size_t toIdx) {
  auto& src = childrenOf(layers_, fromParent);
  if (fromIdx >= src.size()) return;
  auto layer = std::move(src[fromIdx]);
  src.erase(src.begin() + fromIdx);
  // Same-parent forward moves: dst index is in the post-removal frame, just
  // like the existing flat `move(from, to)`. No special-case needed because
  // the caller is expected to have computed `toIdx` against the post-removal
  // layout.
  auto& dst = childrenOf(layers_, toParent);
  if (toIdx > dst.size()) toIdx = dst.size();
  dst.insert(dst.begin() + toIdx, std::move(layer));
}

}  // namespace tuxels
