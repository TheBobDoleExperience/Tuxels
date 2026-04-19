#include <memory>

#include "brush/BrushEngine.h"
#include "brush/RoundBrush.h"
#include "core/TuxImage.h"
#include "history/LayerOpCommand.h"
#include "history/PaintCommand.h"
#include "history/UndoStack.h"
#include "test_harness.h"

namespace tuxels {

TEST(tuximage_cow_records_tile_snapshots) {
  TuxImage img(300, 300);
  img.fill(Rgba32F(0.f, 0.f, 0.f, 1.f));

  // Pre-recording state: capture pointer for tile (0,0).
  auto before00 = img.tiles().sharedAt(TileCoord{0, 0});
  CHECK(before00 != nullptr);

  img.beginRecord();
  img.setPixel(10, 10, Rgba32F(1.f, 0.5f, 0.f, 1.f));
  auto rec = img.stopRecord();

  // Only tile (0,0) was touched (10,10 is inside the first 256×256 tile).
  CHECK_EQ(rec.before.size(), std::size_t{1});
  CHECK_EQ(rec.after.size(), std::size_t{1});

  // Before snapshot points at the original tile — untouched by the write.
  auto beforePtr = rec.before.at(TileCoord{0, 0});
  CHECK(beforePtr.get() == before00.get());
  // After snapshot is a fresh tile distinct from before.
  auto afterPtr = rec.after.at(TileCoord{0, 0});
  CHECK(afterPtr.get() != before00.get());
  // The current tile in the image matches the after snapshot.
  CHECK(img.tiles().sharedAt(TileCoord{0, 0}).get() == afterPtr.get());
}

TEST(paint_command_undo_restores_pixel_values) {
  TuxImage img(10, 10);
  img.fill(Rgba32F(0.2f, 0.2f, 0.2f, 1.f));

  img.beginRecord();
  img.setPixel(5, 5, Rgba32F(1.f, 0.f, 0.f, 1.f));
  auto rec = img.stopRecord();

  CHECK_NEAR(img.getPixel(5, 5).r, 1.f, 1e-5);

  PaintCommand cmd(&img, std::move(rec.before), std::move(rec.after));
  cmd.undo();
  CHECK_NEAR(img.getPixel(5, 5).r, 0.2f, 1e-5);
  cmd.redo();
  CHECK_NEAR(img.getPixel(5, 5).r, 1.f, 1e-5);
}

TEST(brush_stroke_followed_by_undo_reverts_canvas) {
  TuxImage img(60, 20);
  img.fill(Rgba32F(0.f, 0.f, 0.f, 1.f));

  RoundBrushParams p;
  p.diameter = 10;
  p.hardness = 1.f;
  p.opacity = 1.f;
  p.flow = 1.f;
  p.spacingRatio = 0.1f;
  p.color = Rgba32F(1.f, 1.f, 1.f, 1.f);
  RoundBrush brush(p);

  img.beginRecord();
  BrushEngine eng(brush, img);
  eng.beginStroke(10.f, 10.f);
  eng.continueStroke(50.f, 10.f);
  eng.endStroke();
  auto rec = img.stopRecord();

  // Stroke center: painted white.
  CHECK_NEAR(img.getPixel(30, 10).r, 1.f, 1e-4);

  UndoStack stack;
  stack.push(std::make_unique<PaintCommand>(&img, std::move(rec.before),
                                            std::move(rec.after)));

  stack.undo();
  CHECK_NEAR(img.getPixel(30, 10).r, 0.f, 1e-4);

  stack.redo();
  CHECK_NEAR(img.getPixel(30, 10).r, 1.f, 1e-4);
}

TEST(undo_stack_enforces_max_depth) {
  UndoStack stack(/*maxDepth=*/3);
  int counter = 0;
  auto make = [&](int) {
    auto doIt = [&]() { ++counter; };
    auto undoIt = [&]() { --counter; };
    return std::make_unique<LayerOpCommand>("op", std::move(doIt),
                                            std::move(undoIt));
  };
  // Apply and push 5 commands.
  for (int i = 0; i < 5; ++i) {
    auto cmd = make(i);
    cmd->apply();
    stack.push(std::move(cmd));
  }
  CHECK_EQ(stack.undoSize(), std::size_t{3});
  // counter should be 5 (all applied).
  CHECK_EQ(counter, 5);
  // Unwind the 3 that are still in the stack.
  stack.undo();
  stack.undo();
  stack.undo();
  CHECK_EQ(counter, 2);  // oldest 2 commands were pruned from the stack
  CHECK(!stack.canUndo());
  CHECK(stack.canRedo());
}

TEST(undo_stack_push_clears_redo) {
  UndoStack stack;
  int counter = 0;
  auto make = [&]() {
    auto doIt = [&]() { ++counter; };
    auto undoIt = [&]() { --counter; };
    return std::make_unique<LayerOpCommand>("op", std::move(doIt),
                                            std::move(undoIt));
  };
  {
    auto c = make(); c->apply(); stack.push(std::move(c));
  }
  stack.undo();
  CHECK(stack.canRedo());
  {
    auto c = make(); c->apply(); stack.push(std::move(c));
  }
  CHECK(!stack.canRedo());
}

}  // namespace tuxels

int main() { return tuxels::testing::run(); }
