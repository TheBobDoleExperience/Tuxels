#include <memory>
#include <vector>

#include "core/Document.h"
#include "layers/GroupLayer.h"
#include "layers/PixelLayer.h"
#include "test_harness.h"

// Tests the M6-S1 cross-parent layer-move logic at the LayerTree +
// Document level. The drag-and-drop UI (LayersPanel + custom QListWidget
// subclass) translates a drop event into a (movedId, targetParentId,
// targetIndex) tuple; MainWindow then wraps it in a LayerOpCommand whose
// closures call `doc.tree().move(fromP, fromI, toP, toI)`. We exercise the
// closure behavior here (tree-shape transformations + undo) — the Qt
// drop-event plumbing itself is too intricate for non-display tests.

using namespace tuxels;

namespace {

GroupLayer* resolveParent(Document& doc, LayerId pid) {
  if (pid == 0) return nullptr;
  return dynamic_cast<GroupLayer*>(doc.tree().findById(pid));
}

// Mirror of the MainWindow drop closure: erase the layer from its current
// slot, insert it at (toParentId, toIdx). Returns the source slot so the
// caller can run the undo half.
struct DropOp {
  LayerId movedId;
  LayerId fromParentId;
  std::size_t fromIdx;
  LayerId toParentId;
  std::size_t toIdx;
};

DropOp doDrop(Document& doc, LayerId movedId, LayerId toParentId,
              std::size_t toIdx) {
  DropOp op;
  op.movedId = movedId;
  op.toParentId = toParentId;
  op.toIdx = toIdx;
  auto loc = doc.tree().locate(movedId);
  op.fromParentId = (loc && loc->parent) ? loc->parent->id : 0;
  op.fromIdx = loc ? loc->index : 0;
  doc.tree().move(resolveParent(doc, op.fromParentId), op.fromIdx,
                  resolveParent(doc, op.toParentId), op.toIdx);
  doc.setActiveLayerId(movedId);
  return op;
}

void undoDrop(Document& doc, const DropOp& op) {
  doc.tree().move(resolveParent(doc, op.toParentId), op.toIdx,
                  resolveParent(doc, op.fromParentId), op.fromIdx);
  doc.setActiveLayerId(op.movedId);
}

LayerBase* addPixelAt(Document& doc, GroupLayer* parent, const std::string& name) {
  auto pl = std::make_unique<PixelLayer>(/*w=*/8, /*h=*/8);
  pl->name = name;
  pl->id = doc.nextLayerId();
  LayerBase* raw = pl.get();
  if (parent) {
    parent->children.push_back(std::move(pl));
  } else {
    doc.tree().add(std::move(pl));
  }
  return raw;
}

GroupLayer* addGroupAt(Document& doc, GroupLayer* parent, const std::string& name) {
  auto g = std::make_unique<GroupLayer>();
  g->name = name;
  g->id = doc.nextLayerId();
  GroupLayer* raw = g.get();
  if (parent) {
    parent->children.push_back(std::move(g));
  } else {
    doc.tree().add(std::move(g));
  }
  return raw;
}

}  // namespace

// ---------- same-scope reorder ----------

TEST(dnd_same_scope_reorder_root) {
  Document doc(64, 64);
  auto* a = addPixelAt(doc, nullptr, "A");
  auto* b = addPixelAt(doc, nullptr, "B");
  auto* c = addPixelAt(doc, nullptr, "C");
  // Tree: [A@0, B@1, C@2]
  // Move A to top (final tree idx 2).
  auto op = doDrop(doc, a->id, /*toParentId=*/0, /*toIdx=*/2);
  CHECK(doc.tree().at(0)->id == b->id);
  CHECK(doc.tree().at(1)->id == c->id);
  CHECK(doc.tree().at(2)->id == a->id);
  CHECK(doc.activeLayerId() == a->id);
  undoDrop(doc, op);
  CHECK(doc.tree().at(0)->id == a->id);
  CHECK(doc.tree().at(1)->id == b->id);
  CHECK(doc.tree().at(2)->id == c->id);
}

// ---------- cross-scope: root → group ----------

TEST(dnd_root_into_group) {
  Document doc(64, 64);
  auto* a = addPixelAt(doc, nullptr, "A");
  auto* g = addGroupAt(doc, nullptr, "G");
  // Tree: [A@0, G@1{}]
  // Drop A INTO G at end (children.size() = 0 → idx 0).
  auto op = doDrop(doc, a->id, g->id, /*toIdx=*/0);
  CHECK_EQ(static_cast<int>(doc.tree().size()), 1);
  CHECK(doc.tree().at(0)->id == g->id);
  CHECK_EQ(static_cast<int>(g->children.size()), 1);
  CHECK(g->children[0]->id == a->id);
  // Undo: A back at root[0], G at root[1] empty.
  undoDrop(doc, op);
  CHECK_EQ(static_cast<int>(doc.tree().size()), 2);
  CHECK(doc.tree().at(0)->id == a->id);
  CHECK(doc.tree().at(1)->id == g->id);
  CHECK(g->children.empty());
}

// ---------- cross-scope: group → root ----------

TEST(dnd_group_to_root) {
  Document doc(64, 64);
  auto* g = addGroupAt(doc, nullptr, "G");
  auto* inner = addPixelAt(doc, g, "Inner");
  auto* top = addPixelAt(doc, nullptr, "Top");
  // Tree: [G@0{Inner}, Top@1]
  // Drop Inner above Top (final root idx 2 → above Top@1 in panel).
  auto op = doDrop(doc, inner->id, /*toParentId=*/0, /*toIdx=*/2);
  CHECK_EQ(static_cast<int>(doc.tree().size()), 3);
  CHECK(doc.tree().at(0)->id == g->id);
  CHECK(doc.tree().at(1)->id == top->id);
  CHECK(doc.tree().at(2)->id == inner->id);
  CHECK(g->children.empty());
  undoDrop(doc, op);
  CHECK_EQ(static_cast<int>(doc.tree().size()), 2);
  CHECK_EQ(static_cast<int>(g->children.size()), 1);
  CHECK(g->children[0]->id == inner->id);
}

// ---------- cross-scope: group A → group B ----------

TEST(dnd_group_a_to_group_b) {
  Document doc(64, 64);
  auto* gA = addGroupAt(doc, nullptr, "GA");
  auto* gB = addGroupAt(doc, nullptr, "GB");
  auto* x = addPixelAt(doc, gA, "X");
  // Tree: [GA{X}@0, GB{}@1]
  auto op = doDrop(doc, x->id, gB->id, /*toIdx=*/0);
  CHECK(gA->children.empty());
  CHECK_EQ(static_cast<int>(gB->children.size()), 1);
  CHECK(gB->children[0]->id == x->id);
  undoDrop(doc, op);
  CHECK_EQ(static_cast<int>(gA->children.size()), 1);
  CHECK(gA->children[0]->id == x->id);
  CHECK(gB->children.empty());
}

// ---------- nested: drop into group's existing children ----------

TEST(dnd_drop_into_group_with_existing_children) {
  Document doc(64, 64);
  auto* g = addGroupAt(doc, nullptr, "G");
  auto* inner1 = addPixelAt(doc, g, "I1");
  auto* inner2 = addPixelAt(doc, g, "I2");
  auto* outer = addPixelAt(doc, nullptr, "Outer");
  // Tree: [G{I1@0, I2@1}@0, Outer@1]
  // Drop Outer INTO G at end (idx 2 — above I2 in panel under group header).
  auto op = doDrop(doc, outer->id, g->id, g->children.size());
  CHECK_EQ(static_cast<int>(doc.tree().size()), 1);
  CHECK_EQ(static_cast<int>(g->children.size()), 3);
  CHECK(g->children[0]->id == inner1->id);
  CHECK(g->children[1]->id == inner2->id);
  CHECK(g->children[2]->id == outer->id);
  undoDrop(doc, op);
  CHECK_EQ(static_cast<int>(doc.tree().size()), 2);
  CHECK_EQ(static_cast<int>(g->children.size()), 2);
  CHECK(doc.tree().at(1)->id == outer->id);
}

// ---------- same-parent above target panel-row ----------

TEST(dnd_above_target_in_same_parent) {
  Document doc(64, 64);
  auto* a = addPixelAt(doc, nullptr, "A");
  auto* b = addPixelAt(doc, nullptr, "B");
  auto* c = addPixelAt(doc, nullptr, "C");
  auto* d = addPixelAt(doc, nullptr, "D");
  auto* e = addPixelAt(doc, nullptr, "E");
  // Tree: [A@0, B@1, C@2, D@3, E@4]
  // User drops A "above row showing C" in panel. Panel rev order:
  //   E@row0, D@row1, C@row2, B@row3, A@row4.
  // Above C (row 2) → final tree idx = 2 (= K_X). After move:
  //   tree.move(p, 0, p, 2): erase A → [B,C,D,E], insert at 2 → [B,C,A,D,E].
  auto op = doDrop(doc, a->id, /*toParentId=*/0, /*toIdx=*/2);
  CHECK(doc.tree().at(0)->id == b->id);
  CHECK(doc.tree().at(1)->id == c->id);
  CHECK(doc.tree().at(2)->id == a->id);
  CHECK(doc.tree().at(3)->id == d->id);
  CHECK(doc.tree().at(4)->id == e->id);
  undoDrop(doc, op);
  CHECK(doc.tree().at(0)->id == a->id);
  CHECK(doc.tree().at(4)->id == e->id);
}

// ---------- chained moves: undo round-trip after multiple drops ----------

TEST(dnd_chained_drops_undo_round_trip) {
  Document doc(64, 64);
  auto* a = addPixelAt(doc, nullptr, "A");
  auto* b = addPixelAt(doc, nullptr, "B");
  auto* g = addGroupAt(doc, nullptr, "G");
  // Tree: [A@0, B@1, G@2]
  // Drop A into G.
  auto op1 = doDrop(doc, a->id, g->id, 0);
  CHECK_EQ(static_cast<int>(doc.tree().size()), 2);
  CHECK_EQ(static_cast<int>(g->children.size()), 1);
  // Drop B into G after A.
  auto op2 = doDrop(doc, b->id, g->id, 1);
  CHECK_EQ(static_cast<int>(doc.tree().size()), 1);
  CHECK_EQ(static_cast<int>(g->children.size()), 2);
  CHECK(g->children[0]->id == a->id);
  CHECK(g->children[1]->id == b->id);
  // Undo in reverse order.
  undoDrop(doc, op2);
  CHECK_EQ(static_cast<int>(doc.tree().size()), 2);
  CHECK_EQ(static_cast<int>(g->children.size()), 1);
  undoDrop(doc, op1);
  CHECK_EQ(static_cast<int>(doc.tree().size()), 3);
  CHECK_EQ(static_cast<int>(g->children.size()), 0);
  CHECK(doc.tree().at(0)->id == a->id);
  CHECK(doc.tree().at(1)->id == b->id);
  CHECK(doc.tree().at(2)->id == g->id);
}

int main() { return tuxels::testing::run(); }
