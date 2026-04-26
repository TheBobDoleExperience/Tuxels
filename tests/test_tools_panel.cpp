#include <QApplication>
#include <QObject>

#include "test_harness.h"
#include "tools/ToolId.h"
#include "ui/CollapsibleSection.h"
#include "ui/ToolsPanel.h"

namespace tuxels {

namespace {

// All 10 tools should be present as sections in the accordion.
constexpr ToolId kAllTools[] = {
    ToolId::Move,        ToolId::Marquee,  ToolId::Lasso,
    ToolId::PolyLasso,   ToolId::MagicWand, ToolId::SelectByColor,
    ToolId::Crop,        ToolId::Brush,    ToolId::Bucket,
    ToolId::Transform,
};

}  // namespace

TEST(every_tool_has_a_section) {
  ToolsPanel panel;
  for (ToolId id : kAllTools) {
    CHECK(panel.sectionFor(id) != nullptr);
  }
}

TEST(default_active_tool_is_brush) {
  ToolsPanel panel;
  CHECK(panel.sectionFor(ToolId::Brush)->isActive());
  for (ToolId id : kAllTools) {
    if (id == ToolId::Brush) continue;
    CHECK(!panel.sectionFor(id)->isActive());
  }
}

TEST(setActiveTool_highlights_only_one_section) {
  ToolsPanel panel;
  panel.setActiveTool(ToolId::Marquee);
  for (ToolId id : kAllTools) {
    const bool expected = (id == ToolId::Marquee);
    CHECK_EQ(panel.sectionFor(id)->isActive(), expected);
  }
  panel.setActiveTool(ToolId::Lasso);
  for (ToolId id : kAllTools) {
    const bool expected = (id == ToolId::Lasso);
    CHECK_EQ(panel.sectionFor(id)->isActive(), expected);
  }
}

TEST(setActiveTool_does_not_change_expansion_state) {
  ToolsPanel panel;
  // User collapses Brush manually.
  panel.sectionFor(ToolId::Brush)->setExpanded(false);
  CHECK(!panel.sectionFor(ToolId::Brush)->isExpanded());
  // Activate a different tool — Brush stays collapsed.
  panel.setActiveTool(ToolId::Marquee);
  CHECK(!panel.sectionFor(ToolId::Brush)->isExpanded());
  // Activate Brush again — still collapsed (header click only highlights).
  panel.setActiveTool(ToolId::Brush);
  CHECK(!panel.sectionFor(ToolId::Brush)->isExpanded());
}

TEST(section_header_click_emits_toolPicked_with_correct_id) {
  ToolsPanel panel;
  ToolId received = ToolId::Brush;
  int hits = 0;
  QObject::connect(&panel, &ToolsPanel::toolPicked,
                   [&](ToolId id) { received = id; ++hits; });
  panel.sectionFor(ToolId::Marquee)->simulateHeaderClick();
  CHECK_EQ(hits, 1);
  CHECK(received == ToolId::Marquee);

  panel.sectionFor(ToolId::Bucket)->simulateHeaderClick();
  CHECK_EQ(hits, 2);
  CHECK(received == ToolId::Bucket);
}

TEST(setMarqueeMode_does_not_emit_marqueeModeChanged) {
  ToolsPanel panel;
  int hits = 0;
  QObject::connect(&panel, &ToolsPanel::marqueeModeChanged,
                   [&](SelectionMode) { ++hits; });
  panel.setMarqueeMode(SelectionMode::Subtract);
  panel.setMarqueeMode(SelectionMode::Replace);
  CHECK_EQ(hits, 0);
}

TEST(setWandMode_does_not_emit_wandModeChanged) {
  ToolsPanel panel;
  int hits = 0;
  QObject::connect(&panel, &ToolsPanel::wandModeChanged,
                   [&](SelectionMode) { ++hits; });
  panel.setWandMode(SelectionMode::Add);
  panel.setWandMode(SelectionMode::Intersect);
  CHECK_EQ(hits, 0);
}

TEST(setLassoMode_does_not_emit_lassoModeChanged) {
  ToolsPanel panel;
  int hits = 0;
  QObject::connect(&panel, &ToolsPanel::lassoModeChanged,
                   [&](SelectionMode) { ++hits; });
  panel.setLassoMode(SelectionMode::Subtract);
  panel.setLassoMode(SelectionMode::Replace);
  CHECK_EQ(hits, 0);
}

}  // namespace tuxels

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  return tuxels::testing::run();
}
