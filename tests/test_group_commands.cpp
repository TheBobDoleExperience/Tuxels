#include <memory>
#include <set>

#include "compositor/compose.h"
#include "core/Document.h"
#include "core/Histogram.h"
#include "core/TuxImage.h"
#include "layers/GroupLayer.h"
#include "layers/PixelLayer.h"
#include "test_harness.h"

// Tests the M5-S4 Group / Ungroup / Move semantics at the LayerTree +
// Document level. The MainWindow slots wrap these in LayerOpCommand
// closures; the LayerOpCommand pattern itself is exercised by every other
// undoable slot in the codebase, so we focus here on the tree-shape
// transformations that are unique to S4.

using namespace tuxels;

namespace {

GroupLayer* resolveParent(Document& doc, LayerId pid) {
  if (pid == 0) return nullptr;
  return dynamic_cast<GroupLayer*>(doc.tree().findById(pid));
}

// Replicates the do-half of `MainWindow::onLayerGroupActive`. Returns the
// new group's id. Captures parent / idx so the caller can invoke the
// undo half.
struct GroupLayerOp {
  LayerId activeId;
  LayerId parentId;
  std::size_t idx;
  LayerId groupId;
  std::unique_ptr<LayerBase> stashGroup;  // populated by undoOp
};

GroupLayerOp groupLayer(Document& doc, LayerId activeId,
                        const std::string& groupName) {
  GroupLayerOp op;
  op.activeId = activeId;
  op.groupId = doc.nextLayerId();
  auto loc = doc.tree().locate(activeId);
  if (!loc) return op;
  op.parentId = loc->parent ? loc->parent->id : 0;
  op.idx = loc->index;

  GroupLayer* parent = resolveParent(doc, op.parentId);
  auto layer = doc.tree().removeFromPath(parent, op.idx);
  auto g = std::make_unique<GroupLayer>();
  g->id = op.groupId;
  g->name = groupName;
  g->children.push_back(std::move(layer));
  doc.tree().insertAtPath(parent, op.idx, std::move(g));
  doc.setActiveLayerId(op.groupId);
  return op;
}

void undoGroupLayer(Document& doc, GroupLayerOp& op) {
  auto loc = doc.tree().locate(op.groupId);
  if (!loc) return;
  auto group = doc.tree().removeFromPath(loc->parent, loc->index);
  auto* gPtr = static_cast<GroupLayer*>(group.get());
  auto layer = std::move(gPtr->children.front());
  gPtr->children.erase(gPtr->children.begin());
  doc.tree().insertAtPath(resolveParent(doc, op.parentId), op.idx,
                           std::move(layer));
  op.stashGroup = std::move(group);
  doc.setActiveLayerId(op.activeId);
}

void redoGroupLayer(Document& doc, GroupLayerOp& op) {
  GroupLayer* parent = resolveParent(doc, op.parentId);
  auto layer = doc.tree().removeFromPath(parent, op.idx);
  auto* gPtr = static_cast<GroupLayer*>(op.stashGroup.get());
  gPtr->children.insert(gPtr->children.begin(), std::move(layer));
  doc.tree().insertAtPath(parent, op.idx, std::move(op.stashGroup));
  doc.setActiveLayerId(op.groupId);
}

struct UngroupOp {
  LayerId groupId;
  LayerId parentId;
  std::size_t groupIdx;
  std::vector<LayerId> childIds;
  std::unique_ptr<LayerBase> stashGroup;  // populated by doOp
};

UngroupOp ungroupLayer(Document& doc, LayerId groupId) {
  UngroupOp op;
  op.groupId = groupId;
  auto loc = doc.tree().locate(groupId);
  if (!loc) return op;
  op.parentId = loc->parent ? loc->parent->id : 0;
  op.groupIdx = loc->index;
  auto* g = dynamic_cast<GroupLayer*>(doc.tree().findById(groupId));
  if (!g) return op;
  for (const auto& c : g->children) op.childIds.push_back(c->id);

  auto group = doc.tree().removeFromPath(loc->parent, loc->index);
  auto* gPtr = static_cast<GroupLayer*>(group.get());
  GroupLayer* parent = resolveParent(doc, op.parentId);
  for (std::size_t i = 0; i < gPtr->children.size(); ++i) {
    doc.tree().insertAtPath(parent, op.groupIdx + i,
                             std::move(gPtr->children[i]));
  }
  gPtr->children.clear();
  op.stashGroup = std::move(group);

  if (!op.childIds.empty()) {
    doc.setActiveLayerId(op.childIds.front());
  } else {
    LayerId fallback = 0;
    if (parent) {
      if (op.groupIdx < parent->children.size()) {
        fallback = parent->children[op.groupIdx]->id;
      } else if (!parent->children.empty()) {
        fallback = parent->children.back()->id;
      }
    } else {
      if (op.groupIdx < doc.tree().size()) {
        fallback = doc.tree().at(op.groupIdx)->id;
      } else if (!doc.tree().empty()) {
        fallback = doc.tree().at(doc.tree().size() - 1)->id;
      }
    }
    doc.setActiveLayerId(fallback);
  }
  return op;
}

void undoUngroup(Document& doc, UngroupOp& op) {
  if (!op.stashGroup) return;
  auto* gPtr = static_cast<GroupLayer*>(op.stashGroup.get());
  for (LayerId id : op.childIds) {
    auto loc = doc.tree().locate(id);
    if (!loc) continue;
    auto child = doc.tree().removeFromPath(loc->parent, loc->index);
    if (child) gPtr->children.push_back(std::move(child));
  }
  doc.tree().insertAtPath(resolveParent(doc, op.parentId), op.groupIdx,
                           std::move(op.stashGroup));
  doc.setActiveLayerId(op.groupId);
}

}  // namespace

TEST(group_active_layer_wraps_in_group_at_same_index) {
  Document doc(8, 8);
  PixelLayer* a = doc.addBlankPixelLayer("A");
  PixelLayer* b = doc.addBlankPixelLayer("B");
  PixelLayer* c = doc.addBlankPixelLayer("C");
  doc.setActiveLayerId(b->id);
  // [A, B, C] — Group active (B).
  auto op = groupLayer(doc, b->id, "G");
  // Tree becomes [A, group{[B]}, C], active = group.
  CHECK_EQ(doc.tree().size(), std::size_t{3});
  auto* g = dynamic_cast<GroupLayer*>(doc.tree().at(1));
  CHECK(g != nullptr);
  CHECK_EQ(g->children.size(), std::size_t{1});
  CHECK_EQ(g->children[0]->id, b->id);
  CHECK_EQ(doc.activeLayerId(), op.groupId);
  CHECK_EQ(doc.tree().at(0)->id, a->id);
  CHECK_EQ(doc.tree().at(2)->id, c->id);
}

TEST(group_active_layer_undo_restores_original_tree) {
  Document doc(8, 8);
  PixelLayer* a = doc.addBlankPixelLayer("A");
  PixelLayer* b = doc.addBlankPixelLayer("B");
  PixelLayer* c = doc.addBlankPixelLayer("C");
  doc.setActiveLayerId(b->id);
  auto op = groupLayer(doc, b->id, "G");
  undoGroupLayer(doc, op);
  CHECK_EQ(doc.tree().size(), std::size_t{3});
  CHECK_EQ(doc.tree().at(0)->id, a->id);
  CHECK_EQ(doc.tree().at(1)->id, b->id);
  CHECK_EQ(doc.tree().at(2)->id, c->id);
  CHECK_EQ(doc.activeLayerId(), b->id);
  // The group instance is parked in the stash, ready for redo. It still
  // carries its original id (preserved across undo/redo).
  CHECK(op.stashGroup != nullptr);
  CHECK_EQ(op.stashGroup->id, op.groupId);
}

TEST(ungroup_promotes_children_in_order_and_active_to_bottom_child) {
  Document doc(8, 8);
  PixelLayer* a = doc.addBlankPixelLayer("A");
  // Build [A, group{[B, C]}, D] manually.
  auto group = std::make_unique<GroupLayer>();
  group->id = doc.nextLayerId();
  group->name = "G";
  const LayerId groupId = group->id;
  auto b = std::make_unique<PixelLayer>(8, 8);
  b->id = doc.nextLayerId();
  b->name = "B";
  const LayerId bId = b->id;
  auto c = std::make_unique<PixelLayer>(8, 8);
  c->id = doc.nextLayerId();
  c->name = "C";
  const LayerId cId = c->id;
  group->children.push_back(std::move(b));
  group->children.push_back(std::move(c));
  doc.tree().add(std::move(group));
  PixelLayer* d = doc.addBlankPixelLayer("D");

  doc.setActiveLayerId(groupId);
  auto op = ungroupLayer(doc, groupId);
  // Tree becomes [A, B, C, D]; active = bottom-most ex-child = B.
  CHECK_EQ(doc.tree().size(), std::size_t{4});
  CHECK_EQ(doc.tree().at(0)->id, a->id);
  CHECK_EQ(doc.tree().at(1)->id, bId);
  CHECK_EQ(doc.tree().at(2)->id, cId);
  CHECK_EQ(doc.tree().at(3)->id, d->id);
  CHECK_EQ(doc.activeLayerId(), bId);
}

TEST(ungroup_undo_rebuilds_group_with_same_id) {
  Document doc(8, 8);
  PixelLayer* a = doc.addBlankPixelLayer("A");
  auto group = std::make_unique<GroupLayer>();
  group->id = doc.nextLayerId();
  const LayerId groupId = group->id;
  auto b = std::make_unique<PixelLayer>(8, 8);
  b->id = doc.nextLayerId();
  const LayerId bId = b->id;
  auto c = std::make_unique<PixelLayer>(8, 8);
  c->id = doc.nextLayerId();
  const LayerId cId = c->id;
  group->children.push_back(std::move(b));
  group->children.push_back(std::move(c));
  doc.tree().add(std::move(group));
  PixelLayer* d = doc.addBlankPixelLayer("D");
  doc.setActiveLayerId(groupId);

  auto op = ungroupLayer(doc, groupId);
  undoUngroup(doc, op);
  // Back to [A, group{[B, C]}, D].
  CHECK_EQ(doc.tree().size(), std::size_t{3});
  CHECK_EQ(doc.tree().at(0)->id, a->id);
  auto* g2 = dynamic_cast<GroupLayer*>(doc.tree().at(1));
  CHECK(g2 != nullptr);
  CHECK_EQ(g2->id, groupId);
  CHECK_EQ(g2->children.size(), std::size_t{2});
  CHECK_EQ(g2->children[0]->id, bId);
  CHECK_EQ(g2->children[1]->id, cId);
  CHECK_EQ(doc.tree().at(2)->id, d->id);
  CHECK_EQ(doc.activeLayerId(), groupId);
}

TEST(ungroup_empty_group_just_deletes) {
  Document doc(8, 8);
  PixelLayer* a = doc.addBlankPixelLayer("A");
  auto group = std::make_unique<GroupLayer>();
  group->id = doc.nextLayerId();
  const LayerId groupId = group->id;
  doc.tree().add(std::move(group));
  PixelLayer* b = doc.addBlankPixelLayer("B");
  doc.setActiveLayerId(groupId);

  auto op = ungroupLayer(doc, groupId);
  // Tree becomes [A, B]; group is gone.
  CHECK_EQ(doc.tree().size(), std::size_t{2});
  CHECK_EQ(doc.tree().at(0)->id, a->id);
  CHECK_EQ(doc.tree().at(1)->id, b->id);
  // Empty-group fallback: the layer now at the group's old index = b.
  CHECK_EQ(doc.activeLayerId(), b->id);
}

TEST(group_then_ungroup_then_undo_undo_restores_initial) {
  Document doc(8, 8);
  PixelLayer* a = doc.addBlankPixelLayer("A");
  PixelLayer* b = doc.addBlankPixelLayer("B");
  PixelLayer* c = doc.addBlankPixelLayer("C");
  doc.setActiveLayerId(b->id);

  auto gop = groupLayer(doc, b->id, "G");
  auto uop = ungroupLayer(doc, gop.groupId);
  undoUngroup(doc, uop);    // back to [A, group{[B]}, C]
  undoGroupLayer(doc, gop); // back to [A, B, C]

  CHECK_EQ(doc.tree().size(), std::size_t{3});
  CHECK_EQ(doc.tree().at(0)->id, a->id);
  CHECK_EQ(doc.tree().at(1)->id, b->id);
  CHECK_EQ(doc.tree().at(2)->id, c->id);
  CHECK_EQ(doc.activeLayerId(), b->id);
}

TEST(up_down_inside_group_is_scope_local) {
  // Build [Z, group{[X, Y]}, W]. Move X up within the group → group{[Y, X]}.
  // Y stays at index 0 of the group; X at index 1. W and Z don't move.
  Document doc(8, 8);
  doc.addBlankPixelLayer("Z");  // root index 0
  auto group = std::make_unique<GroupLayer>();
  group->id = doc.nextLayerId();
  auto x = std::make_unique<PixelLayer>(8, 8);
  x->id = doc.nextLayerId();
  const LayerId xId = x->id;
  auto y = std::make_unique<PixelLayer>(8, 8);
  y->id = doc.nextLayerId();
  const LayerId yId = y->id;
  GroupLayer* gPtr = group.get();
  group->children.push_back(std::move(x));
  group->children.push_back(std::move(y));
  doc.tree().add(std::move(group));
  PixelLayer* w = doc.addBlankPixelLayer("W");

  // X is at gPtr->children[0]; move up to index 1.
  doc.tree().move(gPtr, /*from=*/0, gPtr, /*to=*/1);
  CHECK_EQ(gPtr->children[0]->id, yId);
  CHECK_EQ(gPtr->children[1]->id, xId);
  // Root size unchanged; group stays at its position.
  CHECK_EQ(doc.tree().size(), std::size_t{3});
  CHECK_EQ(doc.tree().at(0)->name, std::string("Z"));
  CHECK_EQ(doc.tree().at(2)->id, w->id);

  // Top-of-group "no-op" condition is just `idx + 1 >= siblingCount`. X is
  // at index 1 of a 2-child group → can't go further up. Verify the slot's
  // gating logic via the same precondition the slot uses.
  auto loc = doc.tree().locate(xId);
  CHECK(loc.has_value());
  CHECK_EQ(loc->index, std::size_t{1});
  CHECK_EQ(loc->parent->children.size(), std::size_t{2});
  // i.e. `loc->index + 1 >= siblingCount` → no-op.
  CHECK(loc->index + 1 >= loc->parent->children.size());
}

TEST(new_group_inserts_above_active_layer_at_active_idx_plus_one) {
  // [A, B, C], active = B (index 1). New Group lands at index 2 → [A, B, G, C].
  // When active is inside a group, New Group inserts as a sibling at
  // (active + 1) within the same parent.
  Document doc(8, 8);
  doc.addBlankPixelLayer("A");
  PixelLayer* b = doc.addBlankPixelLayer("B");
  doc.addBlankPixelLayer("C");
  doc.setActiveLayerId(b->id);

  // Simulate the slot's insert math.
  auto loc = doc.tree().locate(b->id);
  CHECK(loc.has_value());
  GroupLayer* parent = loc->parent;
  const std::size_t insertIdx = loc->index + 1;
  auto g = std::make_unique<GroupLayer>();
  g->id = doc.nextLayerId();
  g->name = "G";
  const LayerId gId = g->id;
  doc.tree().insertAtPath(parent, insertIdx, std::move(g));
  doc.setActiveLayerId(gId);

  // Verify [A, B, G, C].
  CHECK_EQ(doc.tree().size(), std::size_t{4});
  CHECK_EQ(doc.tree().at(0)->name, std::string("A"));
  CHECK_EQ(doc.tree().at(1)->id, b->id);
  CHECK_EQ(doc.tree().at(2)->id, gId);
  CHECK_EQ(doc.tree().at(3)->name, std::string("C"));
  CHECK_EQ(doc.activeLayerId(), gId);
}

TEST(new_group_inside_existing_group_inserts_as_sibling) {
  // [A, group{[X, Y]}, Z], active = X (inside the group). New Group should
  // land inside the same group at index (X's index + 1) → group{[X, G, Y]}.
  Document doc(8, 8);
  doc.addBlankPixelLayer("A");
  auto group = std::make_unique<GroupLayer>();
  group->id = doc.nextLayerId();
  GroupLayer* gPtr = group.get();
  auto x = std::make_unique<PixelLayer>(8, 8);
  x->id = doc.nextLayerId();
  const LayerId xId = x->id;
  auto y = std::make_unique<PixelLayer>(8, 8);
  y->id = doc.nextLayerId();
  const LayerId yId = y->id;
  group->children.push_back(std::move(x));
  group->children.push_back(std::move(y));
  doc.tree().add(std::move(group));
  doc.addBlankPixelLayer("Z");
  doc.setActiveLayerId(xId);

  // Simulate New Group's insert math at active+1 in the parent.
  auto loc = doc.tree().locate(xId);
  CHECK(loc.has_value());
  CHECK(loc->parent == gPtr);
  CHECK_EQ(loc->index, std::size_t{0});
  auto newG = std::make_unique<GroupLayer>();
  newG->id = doc.nextLayerId();
  const LayerId newGId = newG->id;
  doc.tree().insertAtPath(loc->parent, loc->index + 1, std::move(newG));

  // group's children are now [X, G, Y].
  CHECK_EQ(gPtr->children.size(), std::size_t{3});
  CHECK_EQ(gPtr->children[0]->id, xId);
  CHECK_EQ(gPtr->children[1]->id, newGId);
  CHECK_EQ(gPtr->children[2]->id, yId);
}

// Replicates `MainWindow::histogramBelow` for testing. Hides `target`
// and every layer that follows it in `tree.flatten()` *except* ancestor
// groups (which would otherwise skip-recurse over the group's earlier
// children that we want kept visible). Composes, computes a histogram
// of the result, and restores visibility before returning.
Histogram4x256 histogramBelow(Document& doc, LayerBase* target) {
  std::set<const LayerBase*> ancestors;
  for (LayerId cur = target ? target->id : 0; cur != 0;) {
    auto loc = doc.tree().locate(cur);
    if (!loc || !loc->parent) break;
    ancestors.insert(loc->parent);
    cur = loc->parent->id;
  }
  std::vector<LayerBase*> flat = doc.tree().flatten();
  std::vector<bool> saved;
  saved.reserve(flat.size());
  bool past = false;
  for (LayerBase* l : flat) {
    saved.push_back(l->visible);
    if (l == target) past = true;
    if (past && ancestors.count(l) == 0) l->visible = false;
  }
  TuxImage preview(doc.width(), doc.height());
  compose(doc.tree(), preview);
  for (std::size_t i = 0; i < flat.size(); ++i) flat[i]->visible = saved[i];
  return computeHistogram(preview, doc.selection());
}

TEST(delete_active_group_with_children_round_trip) {
  // Build [A, group{[B, C]}, D]. Delete the group via the same pattern as
  // `MainWindow::onLayerDelete` (locate + removeFromPath). Tree becomes
  // [A, D]; the group + its children are owned by the stash. Undo
  // (insertAtPath) restores everything.
  Document doc(8, 8);
  PixelLayer* a = doc.addBlankPixelLayer("A");
  auto group = std::make_unique<GroupLayer>();
  group->id = doc.nextLayerId();
  const LayerId groupId = group->id;
  auto b = std::make_unique<PixelLayer>(8, 8);
  b->id = doc.nextLayerId();
  const LayerId bId = b->id;
  auto c = std::make_unique<PixelLayer>(8, 8);
  c->id = doc.nextLayerId();
  const LayerId cId = c->id;
  group->children.push_back(std::move(b));
  group->children.push_back(std::move(c));
  doc.tree().add(std::move(group));
  PixelLayer* d = doc.addBlankPixelLayer("D");
  doc.setActiveLayerId(groupId);

  // Delete: capture parent + idx, removeFromPath, set active to next.
  auto loc = doc.tree().locate(groupId);
  CHECK(loc.has_value());
  const LayerId parentId = loc->parent ? loc->parent->id : 0;
  const std::size_t idx = loc->index;
  CHECK_EQ(parentId, LayerId{0});  // group is at root
  auto stash = doc.tree().removeFromPath(resolveParent(doc, parentId), idx);
  CHECK(stash != nullptr);
  // Set active to layer now at parent[idx] = D (which slid into idx 1).
  doc.setActiveLayerId(d->id);

  // Tree shape after delete.
  CHECK_EQ(doc.tree().size(), std::size_t{2});
  CHECK_EQ(doc.tree().at(0)->id, a->id);
  CHECK_EQ(doc.tree().at(1)->id, d->id);
  // Group with both children is still alive in the stash.
  auto* gPtr = static_cast<GroupLayer*>(stash.get());
  CHECK_EQ(gPtr->id, groupId);
  CHECK_EQ(gPtr->children.size(), std::size_t{2});
  CHECK_EQ(gPtr->children[0]->id, bId);
  CHECK_EQ(gPtr->children[1]->id, cId);

  // Undo: re-install the stashed group at original parent + idx.
  doc.tree().insertAtPath(resolveParent(doc, parentId), idx, std::move(stash));
  doc.setActiveLayerId(groupId);
  // Tree restored.
  CHECK_EQ(doc.tree().size(), std::size_t{3});
  CHECK_EQ(doc.tree().at(0)->id, a->id);
  auto* g2 = dynamic_cast<GroupLayer*>(doc.tree().at(1));
  CHECK(g2 != nullptr);
  CHECK_EQ(g2->id, groupId);
  CHECK_EQ(g2->children.size(), std::size_t{2});
  CHECK_EQ(g2->children[0]->id, bId);
  CHECK_EQ(g2->children[1]->id, cId);
  CHECK_EQ(doc.tree().at(2)->id, d->id);
}

TEST(histogram_below_target_inside_passthrough_group) {
  // Build [bg=red, group{PassThrough, [child=green, target_pixel]}].
  // histogramBelow(target_pixel) should hide target + group + everything
  // after target in flatten order. Pass-Through group means children
  // share parent scope, so the "below" composite = [bg, child] = green
  // over red = green wherever child is opaque.
  // For this test, child fills the whole 4x4 doc with green — so the
  // "below" composite is solid green (4x4 = 16 pixels).
  Document doc(4, 4);
  auto bg = std::make_unique<PixelLayer>(4, 4);
  bg->id = doc.nextLayerId();
  bg->image.fill(Rgba32F{1.f, 0.f, 0.f, 1.f});
  doc.tree().add(std::move(bg));

  auto g = std::make_unique<GroupLayer>();
  g->id = doc.nextLayerId();
  g->blend = BlendMode::PassThrough;

  auto child = std::make_unique<PixelLayer>(4, 4);
  child->id = doc.nextLayerId();
  child->image.fill(Rgba32F{0.f, 1.f, 0.f, 1.f});
  g->children.push_back(std::move(child));

  auto target = std::make_unique<PixelLayer>(4, 4);
  target->id = doc.nextLayerId();
  const LayerId targetId = target->id;
  target->image.fill(Rgba32F{0.f, 0.f, 1.f, 1.f});  // blue, would normally show
  g->children.push_back(std::move(target));

  doc.tree().add(std::move(g));

  LayerBase* targetPtr = doc.tree().findById(targetId);
  CHECK(targetPtr != nullptr);
  Histogram4x256 hist = histogramBelow(doc, targetPtr);

  // Composite of [bg, child] = solid green: R=0, G=1, B=0 at 16 pixels.
  // Buckets: R[0]=16, G[255]=16, B[0]=16, luma~bucket 149 (0.587*1*255).
  CHECK_EQ(hist.total, uint64_t{16});
  CHECK_EQ(hist.buckets[0][0], 16u);
  CHECK_EQ(hist.buckets[1][255], 16u);
  CHECK_EQ(hist.buckets[2][0], 16u);
  // Luma channel — solid green should bucket at lround(0.587 * 255) = 150.
  CHECK_EQ(hist.buckets[3][150], 16u);
}

int main() { return ::tuxels::testing::run(); }
