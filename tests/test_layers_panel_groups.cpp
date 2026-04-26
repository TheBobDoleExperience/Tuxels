#include <QApplication>
#include <memory>

#include "core/Document.h"
#include "layers/GroupLayer.h"
#include "layers/PixelLayer.h"
#include "test_harness.h"
#include "ui/LayerRowWidget.h"
#include "ui/LayersPanel.h"

namespace tuxels {

namespace {

// Build a doc with the structure
//   root (top-down): pixel "Top", group "G" expanded { pixel "B", pixel "A" },
//                    pixel "Bottom"
// Display order (PS top-down): Top, G, B, A, Bottom.
// Indent depths:                0,  0, 1, 1, 0.
std::unique_ptr<Document> makeFixture() {
  auto doc = std::make_unique<Document>(8, 8);
  // Bottom-most layer (model index 0) → bottom of UI display.
  doc->addBlankPixelLayer("Bottom");
  // Group "G" with two children. Children added bottom-to-top inside the
  // group: A first (model index 0), B second (model index 1). PS top-down
  // display walks children in reverse, so B appears above A.
  auto g = std::make_unique<GroupLayer>();
  g->id = 100;
  g->name = "G";
  g->isExpanded = true;
  auto a = std::make_unique<PixelLayer>(8, 8);
  a->id = 101;
  a->name = "A";
  g->children.push_back(std::move(a));
  auto b = std::make_unique<PixelLayer>(8, 8);
  b->id = 102;
  b->name = "B";
  g->children.push_back(std::move(b));
  doc->tree().add(std::move(g));
  // Topmost layer (model index 2) — top of UI display.
  doc->addBlankPixelLayer("Top");
  return doc;
}

}  // namespace

TEST(panel_renders_indented_children_top_down) {
  auto doc = makeFixture();
  LayersPanel panel;
  panel.setDocument(doc.get());

  // Display order top-down: Top, G, B, A, Bottom.
  CHECK_EQ(panel.rowCountForTesting(), 5);
  CHECK_EQ(panel.rowAtForTesting(0)->layer()->name, std::string("Top"));
  CHECK_EQ(panel.rowAtForTesting(1)->layer()->name, std::string("G"));
  CHECK_EQ(panel.rowAtForTesting(2)->layer()->name, std::string("B"));
  CHECK_EQ(panel.rowAtForTesting(3)->layer()->name, std::string("A"));
  CHECK_EQ(panel.rowAtForTesting(4)->layer()->name, std::string("Bottom"));
  // Depths.
  CHECK_EQ(panel.rowAtForTesting(0)->indentDepth(), 0);
  CHECK_EQ(panel.rowAtForTesting(1)->indentDepth(), 0);
  CHECK_EQ(panel.rowAtForTesting(2)->indentDepth(), 1);
  CHECK_EQ(panel.rowAtForTesting(3)->indentDepth(), 1);
  CHECK_EQ(panel.rowAtForTesting(4)->indentDepth(), 0);
}

TEST(panel_chevron_collapse_hides_children) {
  auto doc = makeFixture();
  LayersPanel panel;
  panel.setDocument(doc.get());
  // Find the group's row (index 1).
  auto* gRow = panel.rowAtForTesting(1);
  CHECK(gRow != nullptr);
  CHECK(gRow->isChevronVisibleForTesting());

  // Click the chevron — collapses, panel re-renders.
  gRow->simulateChevronClickForTesting();
  // Display now: Top, G (collapsed), Bottom.
  CHECK_EQ(panel.rowCountForTesting(), 3);
  CHECK_EQ(panel.rowAtForTesting(0)->layer()->name, std::string("Top"));
  CHECK_EQ(panel.rowAtForTesting(1)->layer()->name, std::string("G"));
  CHECK_EQ(panel.rowAtForTesting(2)->layer()->name, std::string("Bottom"));

  // isExpanded flipped on the underlying layer.
  auto* g = doc->tree().findById(100);
  auto* gPtr = static_cast<GroupLayer*>(g);
  CHECK(!gPtr->isExpanded);

  // Click again — re-expand, all 5 rows back.
  panel.rowAtForTesting(1)->simulateChevronClickForTesting();
  CHECK_EQ(panel.rowCountForTesting(), 5);
  CHECK(gPtr->isExpanded);
}

TEST(panel_active_highlight_works_for_group_row) {
  auto doc = makeFixture();
  doc->setActiveLayerId(100);  // group "G"
  LayersPanel panel;
  panel.setDocument(doc.get());
  // Group's row should be the active one.
  auto* gRow = panel.rowAtForTesting(1);
  CHECK(gRow->layer()->id == LayerId{100});
  // Verify other rows are not active. The setActive state is reflected in
  // the row's autoFillBackground — set true when active.
  for (int i = 0; i < panel.rowCountForTesting(); ++i) {
    auto* row = panel.rowAtForTesting(i);
    const bool expectedActive = (row->layer()->id == 100);
    CHECK_EQ(row->autoFillBackground(), expectedActive);
  }
}

TEST(panel_group_blend_combo_includes_pass_through) {
  auto doc = makeFixture();
  LayersPanel panel;
  panel.setDocument(doc.get());
  // Group row (index 1) uses kGroupBlendList → 14 entries incl. PassThrough.
  auto* gRow = panel.rowAtForTesting(1);
  CHECK(gRow->blendComboHasPassThroughForTesting());
  CHECK_EQ(gRow->blendItemCountForTesting(), 14);
  // Pixel rows use kPixelBlendList → 13 entries, no PassThrough.
  auto* pxRow = panel.rowAtForTesting(0);
  CHECK(!pxRow->blendComboHasPassThroughForTesting());
  CHECK_EQ(pxRow->blendItemCountForTesting(), 13);
}

TEST(panel_chevron_only_shown_for_groups) {
  auto doc = makeFixture();
  LayersPanel panel;
  panel.setDocument(doc.get());
  CHECK(!panel.rowAtForTesting(0)->isChevronVisibleForTesting());  // pixel
  CHECK(panel.rowAtForTesting(1)->isChevronVisibleForTesting());   // group
  CHECK(!panel.rowAtForTesting(2)->isChevronVisibleForTesting());  // child
  CHECK(!panel.rowAtForTesting(4)->isChevronVisibleForTesting());  // pixel
}

TEST(panel_collapsed_group_walks_visible_only) {
  // Children of a collapsed group don't appear, regardless of how many
  // they are or whether they're nested groups themselves.
  auto doc = std::make_unique<Document>(8, 8);
  auto outer = std::make_unique<GroupLayer>();
  outer->id = 1;
  outer->name = "outer";
  outer->isExpanded = false;
  auto inner = std::make_unique<GroupLayer>();
  inner->id = 2;
  inner->name = "inner";
  inner->isExpanded = true;
  auto px = std::make_unique<PixelLayer>(8, 8);
  px->id = 3;
  px->name = "deep";
  inner->children.push_back(std::move(px));
  outer->children.push_back(std::move(inner));
  doc->tree().add(std::move(outer));

  LayersPanel panel;
  panel.setDocument(doc.get());
  // Outer is collapsed → only "outer" appears, no children.
  CHECK_EQ(panel.rowCountForTesting(), 1);
  CHECK_EQ(panel.rowAtForTesting(0)->layer()->name, std::string("outer"));
}

}  // namespace tuxels

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  return tuxels::testing::run();
}
