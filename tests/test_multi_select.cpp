#include <memory>
#include <vector>

#include "core/Document.h"
#include "layers/GroupLayer.h"
#include "layers/PixelLayer.h"
#include "test_harness.h"

// Tests M6-S2 multi-select infrastructure at the Document level + the
// batch-op closure semantics that MainWindow runs through LayerOpCommand.
// The Qt panel-side selection plumbing (ExtendedSelection, Shift / Ctrl
// modifier handling) is delegated to QListWidget and isn't exercised here;
// we focus on the data-model contract that batch ops depend on.

using namespace tuxels;

namespace {

GroupLayer* resolveParent(Document& doc, LayerId pid) {
  if (pid == 0) return nullptr;
  return dynamic_cast<GroupLayer*>(doc.tree().findById(pid));
}

LayerBase* addPixel(Document& doc, GroupLayer* parent, const std::string& name) {
  auto pl = std::make_unique<PixelLayer>(8, 8);
  pl->name = name;
  pl->id = doc.nextLayerId();
  LayerBase* raw = pl.get();
  if (parent) parent->children.push_back(std::move(pl));
  else doc.tree().add(std::move(pl));
  return raw;
}

GroupLayer* addGroup(Document& doc, GroupLayer* parent, const std::string& name) {
  auto g = std::make_unique<GroupLayer>();
  g->name = name;
  g->id = doc.nextLayerId();
  GroupLayer* raw = g.get();
  if (parent) parent->children.push_back(std::move(g));
  else doc.tree().add(std::move(g));
  return raw;
}

// ---------- Batch delete closure (mirrors MainWindow::onLayerDelete batch path) ----------

struct DeleteRecord {
  LayerId id;
  LayerId parentId;
  std::size_t idx;
  std::unique_ptr<LayerBase> ptr;
};

std::vector<LayerId> filterDescendants(Document& doc,
                                        const std::vector<LayerBase*>& sel) {
  std::vector<LayerId> out;
  for (LayerBase* layer : sel) {
    bool ancestorSelected = false;
    auto curLoc = doc.tree().locate(layer->id);
    GroupLayer* p = curLoc ? curLoc->parent : nullptr;
    while (p != nullptr && !ancestorSelected) {
      for (LayerBase* s : sel) {
        if (s->id == p->id) { ancestorSelected = true; break; }
      }
      if (ancestorSelected) break;
      auto pLoc = doc.tree().locate(p->id);
      p = pLoc ? pLoc->parent : nullptr;
    }
    if (!ancestorSelected) out.push_back(layer->id);
  }
  return out;
}

std::vector<DeleteRecord> doBatchDelete(Document& doc,
                                         const std::vector<LayerId>& ids) {
  std::vector<DeleteRecord> out;
  for (LayerId id : ids) {
    auto loc = doc.tree().locate(id);
    if (!loc) continue;
    LayerId parentId = loc->parent ? loc->parent->id : 0;
    std::size_t idx = loc->index;
    auto ptr = doc.tree().removeFromPath(loc->parent, idx);
    if (ptr) out.push_back({id, parentId, idx, std::move(ptr)});
  }
  return out;
}

void undoBatchDelete(Document& doc, std::vector<DeleteRecord>& stashes) {
  for (auto it = stashes.rbegin(); it != stashes.rend(); ++it) {
    if (it->ptr) {
      doc.tree().insertAtPath(resolveParent(doc, it->parentId), it->idx,
                                std::move(it->ptr));
    }
  }
}

}  // namespace

// ---------- Document selection set ----------

TEST(selection_set_default_empty) {
  Document doc(64, 64);
  CHECK(doc.selectedLayerIds().empty());
}

TEST(selection_set_set_and_get) {
  Document doc(64, 64);
  auto* a = addPixel(doc, nullptr, "A");
  auto* b = addPixel(doc, nullptr, "B");
  doc.setSelectedLayerIds({a->id, b->id});
  const auto& ids = doc.selectedLayerIds();
  CHECK_EQ(static_cast<int>(ids.size()), 2);
  CHECK(ids[0] == a->id);
  CHECK(ids[1] == b->id);
  auto layers = doc.selectedLayers();
  CHECK_EQ(static_cast<int>(layers.size()), 2);
  CHECK(layers[0] == a);
  CHECK(layers[1] == b);
}

TEST(selection_set_filters_invalid_ids) {
  Document doc(64, 64);
  auto* a = addPixel(doc, nullptr, "A");
  doc.setSelectedLayerIds({a->id, 9999});  // 9999 doesn't exist
  auto layers = doc.selectedLayers();
  CHECK_EQ(static_cast<int>(layers.size()), 1);
  CHECK(layers[0] == a);
}

// ---------- Batch delete ----------

TEST(batch_delete_root_layers_undo_round_trip) {
  Document doc(64, 64);
  auto* a = addPixel(doc, nullptr, "A");
  auto* b = addPixel(doc, nullptr, "B");
  auto* c = addPixel(doc, nullptr, "C");
  // Tree: [A, B, C]. Select {A, C}. Delete should leave [B].
  doc.setSelectedLayerIds({a->id, c->id});
  auto sel = doc.selectedLayers();
  auto filtered = filterDescendants(doc, sel);
  auto stashes = doBatchDelete(doc, filtered);
  CHECK_EQ(static_cast<int>(doc.tree().size()), 1);
  CHECK(doc.tree().at(0)->id == b->id);
  // Undo restores [A, B, C].
  undoBatchDelete(doc, stashes);
  CHECK_EQ(static_cast<int>(doc.tree().size()), 3);
  CHECK(doc.tree().at(0)->id == a->id);
  CHECK(doc.tree().at(1)->id == b->id);
  CHECK(doc.tree().at(2)->id == c->id);
}

TEST(batch_delete_filters_descendants_of_selected_group) {
  Document doc(64, 64);
  auto* a = addPixel(doc, nullptr, "A");
  auto* g = addGroup(doc, nullptr, "G");
  auto* b = addPixel(doc, g, "B");
  auto* c = addPixel(doc, g, "C");
  auto* d = addPixel(doc, nullptr, "D");
  (void)a;
  (void)d;
  // Tree: [A, G{B,C}, D]. Select {B, G}. B is descendant of G → filter out.
  doc.setSelectedLayerIds({b->id, g->id});
  auto sel = doc.selectedLayers();
  auto filtered = filterDescendants(doc, sel);
  CHECK_EQ(static_cast<int>(filtered.size()), 1);
  CHECK(filtered[0] == g->id);
  // Delete G — children come along inside its unique_ptr.
  auto stashes = doBatchDelete(doc, filtered);
  CHECK_EQ(static_cast<int>(doc.tree().size()), 2);
  // Undo restores G with B + C.
  undoBatchDelete(doc, stashes);
  CHECK_EQ(static_cast<int>(doc.tree().size()), 3);
  auto* restoredG = dynamic_cast<GroupLayer*>(doc.tree().at(1));
  CHECK(restoredG != nullptr);
  CHECK_EQ(static_cast<int>(restoredG->children.size()), 2);
  CHECK(restoredG->children[0]->id == b->id);
  CHECK(restoredG->children[1]->id == c->id);
}

TEST(batch_delete_mixed_root_and_group_child) {
  Document doc(64, 64);
  auto* a = addPixel(doc, nullptr, "A");
  auto* g = addGroup(doc, nullptr, "G");
  auto* b = addPixel(doc, g, "B");
  auto* c = addPixel(doc, g, "C");
  // Tree: [A, G{B, C}]. Select {A, B}. B is in G but G isn't selected.
  doc.setSelectedLayerIds({a->id, b->id});
  auto sel = doc.selectedLayers();
  auto filtered = filterDescendants(doc, sel);
  CHECK_EQ(static_cast<int>(filtered.size()), 2);
  auto stashes = doBatchDelete(doc, filtered);
  CHECK_EQ(static_cast<int>(doc.tree().size()), 1);
  CHECK(doc.tree().at(0)->id == g->id);
  CHECK_EQ(static_cast<int>(g->children.size()), 1);
  CHECK(g->children[0]->id == c->id);
  // Undo restores both.
  undoBatchDelete(doc, stashes);
  CHECK_EQ(static_cast<int>(doc.tree().size()), 2);
  CHECK_EQ(static_cast<int>(g->children.size()), 2);
  CHECK(g->children[0]->id == b->id);
  CHECK(g->children[1]->id == c->id);
}

// ---------- Batch group ----------

TEST(batch_group_root_layers_collects_in_bottom_up_order) {
  Document doc(64, 64);
  auto* a = addPixel(doc, nullptr, "A");
  auto* b = addPixel(doc, nullptr, "B");
  auto* c = addPixel(doc, nullptr, "C");
  // Tree: [A@0, B@1, C@2]. Select {A, C}. Group → [B@0, NewGroup{A, C}@1].
  doc.setSelectedLayerIds({a->id, c->id});
  auto sel = doc.selectedLayers();
  auto filtered = filterDescendants(doc, sel);

  // Collect ordered ids via flatten() (mirrors MainWindow's batch group).
  std::vector<LayerId> ordered;
  for (LayerBase* l : doc.tree().flatten()) {
    for (LayerId id : filtered) {
      if (l->id == id) { ordered.push_back(id); break; }
    }
  }
  CHECK_EQ(static_cast<int>(ordered.size()), 2);
  CHECK(ordered[0] == a->id);  // A first (lower tree idx)
  CHECK(ordered[1] == c->id);  // C second

  // Apply: remove all in order, capture last slot, insert new group.
  std::vector<std::unique_ptr<LayerBase>> children;
  LayerId lastParentId = 0;
  std::size_t lastIdx = 0;
  for (LayerId id : ordered) {
    auto loc = doc.tree().locate(id);
    if (!loc) continue;
    lastParentId = loc->parent ? loc->parent->id : 0;
    lastIdx = loc->index;
    auto ptr = doc.tree().removeFromPath(loc->parent, lastIdx);
    if (ptr) children.push_back(std::move(ptr));
  }
  auto group = std::make_unique<GroupLayer>();
  group->id = doc.nextLayerId();
  group->children = std::move(children);
  doc.tree().insertAtPath(resolveParent(doc, lastParentId), lastIdx,
                            std::move(group));

  CHECK_EQ(static_cast<int>(doc.tree().size()), 2);
  CHECK(doc.tree().at(0)->id == b->id);
  auto* newG = dynamic_cast<GroupLayer*>(doc.tree().at(1));
  CHECK(newG != nullptr);
  CHECK_EQ(static_cast<int>(newG->children.size()), 2);
  CHECK(newG->children[0]->id == a->id);
  CHECK(newG->children[1]->id == c->id);
}

// ---------- Batch visibility toggle ----------

TEST(batch_visibility_toggle_per_layer_old_vals_for_undo) {
  Document doc(64, 64);
  auto* a = addPixel(doc, nullptr, "A");
  auto* b = addPixel(doc, nullptr, "B");
  auto* c = addPixel(doc, nullptr, "C");
  a->visible = true;
  b->visible = false;  // mixed initial state
  c->visible = true;
  doc.setSelectedLayerIds({a->id, b->id, c->id});

  // Capture per-layer oldVal, then toggle all to newVal=false.
  std::vector<bool> oldVals;
  for (LayerId id : doc.selectedLayerIds()) {
    auto* l = doc.tree().findById(id);
    oldVals.push_back(l ? l->visible : false);
  }
  const bool newVal = false;
  for (LayerId id : doc.selectedLayerIds()) {
    if (auto* l = doc.tree().findById(id)) l->visible = newVal;
  }
  CHECK(a->visible == false);
  CHECK(b->visible == false);
  CHECK(c->visible == false);

  // Undo.
  for (std::size_t i = 0; i < doc.selectedLayerIds().size(); ++i) {
    if (auto* l = doc.tree().findById(doc.selectedLayerIds()[i])) {
      l->visible = oldVals[i];
    }
  }
  CHECK(a->visible == true);
  CHECK(b->visible == false);  // restored to its original false
  CHECK(c->visible == true);
}

int main() { return tuxels::testing::run(); }
