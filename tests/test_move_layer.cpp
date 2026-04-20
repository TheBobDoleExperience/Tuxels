#include <cmath>
#include <memory>

#include "compositor/compose.h"
#include "core/Document.h"
#include "core/Pixel.h"
#include "core/TuxImage.h"
#include "history/MoveLayerCommand.h"
#include "layers/PixelLayer.h"
#include "test_harness.h"
#include "tools/MoveTool.h"
#include "tools/ToolBase.h"

using namespace tuxels;

namespace {

constexpr Rgba32F kRed{1.f, 0.f, 0.f, 1.f};
constexpr Rgba32F kGreen{0.f, 1.f, 0.f, 1.f};

PixelLayer* addSolidLayer(Document& doc, int w, int h, Rgba32F c, int ox,
                          int oy, const char* name) {
  TuxImage img(w, h);
  img.fill(c);
  return doc.addPixelLayer(std::move(img), ox, oy, name);
}

}  // namespace

TEST(move_tool_press_move_release_shifts_origin) {
  Document doc(100, 100);
  auto* layer = addSolidLayer(doc, 20, 20, kGreen, 30, 40, "Green");
  const LayerId id = layer->id;

  MoveTool tool;
  tool.press(doc, 35.f, 45.f, MouseButton::Left);
  // Drag the cursor +10 right, +7 down — origin should match the delta.
  tool.move(doc, 45.f, 52.f);
  CHECK_EQ(layer->originX, 40);
  CHECK_EQ(layer->originY, 47);
  tool.release(doc, 45.f, 52.f, MouseButton::Left);
  auto commit = tool.takeCommit();
  CHECK(commit.has_value());
  CHECK_EQ(commit->layerId, id);
  CHECK_EQ(commit->beforeX, 30);
  CHECK_EQ(commit->beforeY, 40);
  CHECK_EQ(commit->afterX, 40);
  CHECK_EQ(commit->afterY, 47);
}

TEST(move_tool_zero_delta_release_produces_no_commit) {
  // Press-release without moving shouldn't push an undoable move.
  Document doc(50, 50);
  addSolidLayer(doc, 10, 10, kRed, 5, 5, "R");
  MoveTool tool;
  tool.press(doc, 6.f, 6.f, MouseButton::Left);
  tool.release(doc, 6.f, 6.f, MouseButton::Left);
  CHECK(!tool.takeCommit().has_value());
}

TEST(move_tool_dirty_rect_unions_before_and_after_doc_extents) {
  // A 40×40 layer centered in a 100×100 doc dragged +20, -10 — the dirty
  // rect reported to the canvas should cover both footprints so the
  // compositor repaints both the vacated and newly-filled regions.
  Document doc(100, 100);
  addSolidLayer(doc, 40, 40, kGreen, 30, 30, "Block");  // doc rect [30..70)
  MoveTool tool;
  tool.press(doc, 50.f, 50.f, MouseButton::Left);
  tool.move(doc, 70.f, 40.f);  // new origin (50, 20) → doc rect [50..90)×[20..60)
  Rect d = tool.takeDirtyRect();
  // Union of [30..70)×[30..70) and [50..90)×[20..60) clipped to the doc
  // (which fully contains both) → [30..90) × [20..70).
  CHECK_EQ(d.x, 30);
  CHECK_EQ(d.y, 20);
  CHECK_EQ(d.w, 60);
  CHECK_EQ(d.h, 50);
}

TEST(move_tool_ignores_non_pixel_active_layer_gracefully) {
  // No active layer → press is a no-op; release should produce no commit
  // and no crash.
  Document doc(50, 50);
  MoveTool tool;
  tool.press(doc, 10.f, 10.f, MouseButton::Left);
  tool.move(doc, 20.f, 20.f);
  tool.release(doc, 20.f, 20.f, MouseButton::Left);
  CHECK(!tool.takeCommit().has_value());
}

TEST(move_layer_command_swaps_origin_on_undo_redo) {
  Document doc(100, 100);
  auto* bg = addSolidLayer(doc, 100, 100, kRed, 0, 0, "BG");
  auto* block = addSolidLayer(doc, 20, 20, kGreen, 10, 10, "Block");
  doc.setActiveLayerIndex(1);
  (void)bg;

  // Simulate a drag result: layer was at (10,10), moved to (40,50).
  block->originX = 40;
  block->originY = 50;
  MoveLayerCommand cmd(&doc, block->id, 10, 10, 40, 50);

  // Composite shows green at (40..60)×(50..70), red elsewhere.
  TuxImage out(100, 100);
  compose(doc.tree(), out);
  CHECK(approxEqual(out.getPixel(40, 50), kGreen));
  CHECK(approxEqual(out.getPixel(59, 69), kGreen));
  CHECK(approxEqual(out.getPixel(10, 10), kRed));  // old spot reverted to bg

  // Undo → origin back to (10,10), composite reveals green there again.
  cmd.undo();
  CHECK_EQ(block->originX, 10);
  CHECK_EQ(block->originY, 10);
  compose(doc.tree(), out);
  CHECK(approxEqual(out.getPixel(10, 10), kGreen));
  CHECK(approxEqual(out.getPixel(40, 50), kRed));

  // Redo → origin back to (40,50).
  cmd.redo();
  CHECK_EQ(block->originX, 40);
  CHECK_EQ(block->originY, 50);
  compose(doc.tree(), out);
  CHECK(approxEqual(out.getPixel(40, 50), kGreen));
  CHECK(approxEqual(out.getPixel(10, 10), kRed));
}

TEST(move_layer_command_finds_layer_by_id_not_by_index) {
  // Command holds an id, not an index — a reorder between commit and
  // undo/redo must still route to the correct layer.
  Document doc(100, 100);
  auto* a = addSolidLayer(doc, 100, 100, kRed, 0, 0, "A");
  auto* b = addSolidLayer(doc, 20, 20, kGreen, 10, 10, "B");
  MoveLayerCommand cmd(&doc, b->id, 10, 10, 50, 50);

  // Apply installs after.
  cmd.redo();
  CHECK_EQ(b->originX, 50);
  CHECK_EQ(b->originY, 50);

  // Reorder the tree: swap the two layers.
  doc.tree().move(1, 0);  // B now at index 0, A at index 1
  CHECK_EQ(doc.tree().at(1), a);

  // Undo still targets B by id.
  cmd.undo();
  CHECK_EQ(b->originX, 10);
  CHECK_EQ(b->originY, 10);
  CHECK_EQ(a->originX, 0);  // unaffected
  CHECK_EQ(a->originY, 0);
}

int main() { return tuxels::testing::run(); }
