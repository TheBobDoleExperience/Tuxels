#pragma once

#include <string>
#include <utility>

#include "core/TuxImage.h"
#include "history/Command.h"

namespace tuxels {

// Captures a paint stroke as a pair of tile snapshots (pre / post). Undo
// swaps in the pre snapshot; redo swaps in the post snapshot. Relies on
// TuxImage's COW recording window so touched tiles have distinct pointers
// from their pre-stroke state.
class PaintCommand : public Command {
 public:
  PaintCommand(TuxImage* target, TuxImage::TileSnapshot before,
               TuxImage::TileSnapshot after, std::string label = "Paint")
      : target_(target),
        before_(std::move(before)),
        after_(std::move(after)),
        label_(std::move(label)) {}

  void undo() override { applySnapshot(before_); }
  void redo() override { applySnapshot(after_); }
  const std::string& label() const override { return label_; }

 private:
  void applySnapshot(const TuxImage::TileSnapshot& snap) {
    if (!target_) return;
    for (const auto& [tc, ptr] : snap) {
      target_->tiles().set(tc, ptr);
    }
  }

  TuxImage* target_;
  TuxImage::TileSnapshot before_;
  TuxImage::TileSnapshot after_;
  std::string label_;
};

}  // namespace tuxels
