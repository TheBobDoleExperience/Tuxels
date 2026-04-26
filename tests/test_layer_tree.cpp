#include <memory>

#include "core/Document.h"
#include "layers/GroupLayer.h"
#include "layers/LayerBase.h"
#include "layers/LayerTree.h"
#include "layers/PixelLayer.h"
#include "test_harness.h"

using namespace tuxels;

namespace {

// Hand-construct a fixture without going through Document so the tree's
// recursive helpers can be exercised independently of any UI plumbing.
//
//   root: [A, outer, F]
//   outer: GroupLayer { children: [B, inner, E] }
//   inner: GroupLayer { children: [C, D] }
struct NestedFixture {
  LayerTree tree;
  LayerBase* A = nullptr;
  LayerBase* B = nullptr;
  LayerBase* C = nullptr;
  LayerBase* D = nullptr;
  LayerBase* E = nullptr;
  LayerBase* F = nullptr;
  GroupLayer* outer = nullptr;
  GroupLayer* inner = nullptr;
};

NestedFixture makeNested() {
  NestedFixture f;
  auto a = std::make_unique<PixelLayer>(2, 2);
  a->id = 1; a->name = "A";
  f.A = a.get();
  auto outer = std::make_unique<GroupLayer>();
  outer->id = 10; outer->name = "outer";
  f.outer = outer.get();
  auto b = std::make_unique<PixelLayer>(2, 2);
  b->id = 2; b->name = "B";
  f.B = b.get();
  auto inner = std::make_unique<GroupLayer>();
  inner->id = 11; inner->name = "inner";
  f.inner = inner.get();
  auto c = std::make_unique<PixelLayer>(2, 2);
  c->id = 3; c->name = "C";
  f.C = c.get();
  auto d = std::make_unique<PixelLayer>(2, 2);
  d->id = 4; d->name = "D";
  f.D = d.get();
  inner->children.push_back(std::move(c));
  inner->children.push_back(std::move(d));
  auto e = std::make_unique<PixelLayer>(2, 2);
  e->id = 5; e->name = "E";
  f.E = e.get();
  outer->children.push_back(std::move(b));
  outer->children.push_back(std::move(inner));
  outer->children.push_back(std::move(e));
  auto fff = std::make_unique<PixelLayer>(2, 2);
  fff->id = 6; fff->name = "F";
  f.F = fff.get();
  f.tree.add(std::move(a));
  f.tree.add(std::move(outer));
  f.tree.add(std::move(fff));
  return f;
}

}  // namespace

TEST(layer_tree_flat_findById_walks_root) {
  LayerTree t;
  auto a = std::make_unique<PixelLayer>(2, 2);
  a->id = 11;
  auto b = std::make_unique<PixelLayer>(2, 2);
  b->id = 22;
  auto c = std::make_unique<PixelLayer>(2, 2);
  c->id = 33;
  LayerBase* aRaw = a.get();
  LayerBase* bRaw = b.get();
  LayerBase* cRaw = c.get();
  t.add(std::move(a));
  t.add(std::move(b));
  t.add(std::move(c));
  CHECK(t.findById(11) == aRaw);
  CHECK(t.findById(22) == bRaw);
  CHECK(t.findById(33) == cRaw);
  CHECK(t.findById(99) == nullptr);
}

TEST(layer_tree_nested_findById_descends) {
  auto f = makeNested();
  CHECK(f.tree.findById(1) == f.A);
  CHECK(f.tree.findById(2) == f.B);
  CHECK(f.tree.findById(3) == f.C);
  CHECK(f.tree.findById(4) == f.D);
  CHECK(f.tree.findById(5) == f.E);
  CHECK(f.tree.findById(6) == f.F);
  CHECK(f.tree.findById(10) == f.outer);
  CHECK(f.tree.findById(11) == f.inner);
  CHECK(f.tree.findById(999) == nullptr);
}

TEST(layer_tree_locate_returns_parent_and_index) {
  auto f = makeNested();
  // Root-level: parent == nullptr.
  auto la = f.tree.locate(1);
  CHECK(la.has_value());
  CHECK(la->parent == nullptr);
  CHECK_EQ(la->index, std::size_t{0});
  auto louter = f.tree.locate(10);
  CHECK(louter.has_value());
  CHECK(louter->parent == nullptr);
  CHECK_EQ(louter->index, std::size_t{1});
  auto lf = f.tree.locate(6);
  CHECK(lf.has_value());
  CHECK(lf->parent == nullptr);
  CHECK_EQ(lf->index, std::size_t{2});
  // Inside outer.
  auto lb = f.tree.locate(2);
  CHECK(lb.has_value());
  CHECK(lb->parent == f.outer);
  CHECK_EQ(lb->index, std::size_t{0});
  auto linner = f.tree.locate(11);
  CHECK(linner.has_value());
  CHECK(linner->parent == f.outer);
  CHECK_EQ(linner->index, std::size_t{1});
  auto le = f.tree.locate(5);
  CHECK(le.has_value());
  CHECK(le->parent == f.outer);
  CHECK_EQ(le->index, std::size_t{2});
  // Inside inner.
  auto lc = f.tree.locate(3);
  CHECK(lc.has_value());
  CHECK(lc->parent == f.inner);
  CHECK_EQ(lc->index, std::size_t{0});
  auto ld = f.tree.locate(4);
  CHECK(ld.has_value());
  CHECK(ld->parent == f.inner);
  CHECK_EQ(ld->index, std::size_t{1});
  CHECK(!f.tree.locate(999).has_value());
}

TEST(layer_tree_flatten_depth_first_child_then_self) {
  auto f = makeNested();
  std::vector<LayerBase*> got = f.tree.flatten();
  // Child-then-self: A then [B, [C, D, inner], E, outer], F.
  std::vector<LayerBase*> want = {f.A, f.B, f.C, f.D, f.inner, f.E, f.outer,
                                   f.F};
  CHECK_EQ(got.size(), want.size());
  for (std::size_t i = 0; i < want.size(); ++i) {
    CHECK(got[i] == want[i]);
  }
}

TEST(layer_tree_insertAtPath_root_and_into_group) {
  auto f = makeNested();
  // Insert at root index 0 (below A).
  auto z = std::make_unique<PixelLayer>(2, 2);
  z->id = 77; z->name = "Z";
  LayerBase* zRaw = z.get();
  f.tree.insertAtPath(/*parent=*/nullptr, /*index=*/0, std::move(z));
  auto loc = f.tree.locate(77);
  CHECK(loc.has_value());
  CHECK(loc->parent == nullptr);
  CHECK_EQ(loc->index, std::size_t{0});
  CHECK(zRaw->name == "Z");
  // Insert into the inner group between C and D (index 1).
  auto m = std::make_unique<PixelLayer>(2, 2);
  m->id = 88;
  f.tree.insertAtPath(f.inner, /*index=*/1, std::move(m));
  auto lm = f.tree.locate(88);
  CHECK(lm.has_value());
  CHECK(lm->parent == f.inner);
  CHECK_EQ(lm->index, std::size_t{1});
  // Old C still at 0, D moved to 2.
  auto lc = f.tree.locate(3);
  CHECK_EQ(lc->index, std::size_t{0});
  auto ld = f.tree.locate(4);
  CHECK_EQ(ld->index, std::size_t{2});
}

TEST(layer_tree_removeFromPath_returns_unique_ptr) {
  auto f = makeNested();
  // Pull C out of the inner group.
  auto extracted = f.tree.removeFromPath(f.inner, /*index=*/0);
  CHECK(extracted.get() != nullptr);
  CHECK_EQ(extracted->id, LayerId{3});
  // findById no longer sees it.
  CHECK(f.tree.findById(3) == nullptr);
  // D's index slid from 1 to 0.
  auto ld = f.tree.locate(4);
  CHECK_EQ(ld->index, std::size_t{0});
}

TEST(layer_tree_move_cross_group) {
  auto f = makeNested();
  // Move B (outer's child 0) out to the root (between outer and F = index 2).
  f.tree.move(/*from*/ f.outer, /*fromIdx*/ 0,
              /*to*/ nullptr, /*toIdx*/ 2);
  // B is now at root index 2; F is now at root index 3.
  auto lb = f.tree.locate(2);
  CHECK(lb.has_value());
  CHECK(lb->parent == nullptr);
  CHECK_EQ(lb->index, std::size_t{2});
  auto lf = f.tree.locate(6);
  CHECK_EQ(lf->index, std::size_t{3});
  // outer's children shrank by one — inner is now child 0, E child 1.
  auto linner = f.tree.locate(11);
  CHECK(linner->parent == f.outer);
  CHECK_EQ(linner->index, std::size_t{0});
  auto le = f.tree.locate(5);
  CHECK_EQ(le->index, std::size_t{1});
  // Ids preserved across the move.
  CHECK(f.tree.findById(2) != nullptr);
}

TEST(layer_tree_forEach_visits_all_nodes) {
  auto f = makeNested();
  int count = 0;
  bool sawOuter = false, sawInner = false, sawC = false;
  f.tree.forEach([&](LayerBase* l) {
    ++count;
    if (l == f.outer) sawOuter = true;
    if (l == f.inner) sawInner = true;
    if (l == f.C) sawC = true;
  });
  // 6 leaf layers + 2 groups = 8 visits.
  CHECK_EQ(count, 8);
  CHECK(sawOuter);
  CHECK(sawInner);
  CHECK(sawC);
}

TEST(document_active_layer_id_survives_reorder) {
  Document doc(8, 8);
  PixelLayer* a = doc.addBlankPixelLayer("A");
  PixelLayer* b = doc.addBlankPixelLayer("B");
  PixelLayer* c = doc.addBlankPixelLayer("C");
  doc.setActiveLayerId(b->id);
  CHECK(doc.activeLayer() == b);
  // Reorder via flat move: [A,B,C] → [B,C,A].
  doc.tree().move(0, 2);
  // Active id still resolves to B even though indices changed.
  CHECK(doc.activeLayer() == b);
  CHECK_EQ(doc.activeLayerId(), b->id);
  // Index shim now reports B's new position (index 0).
  CHECK_EQ(doc.activeLayerIndex(), 0);
  (void)a;
  (void)c;
}

int main() { return ::tuxels::testing::run(); }
