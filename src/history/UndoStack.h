#pragma once

#include <cstddef>
#include <deque>
#include <memory>

#include "history/Command.h"

namespace tuxels {

class UndoStack {
 public:
  explicit UndoStack(std::size_t maxDepth = 64) : maxDepth_(maxDepth) {}

  // Push a command whose side-effect has already been applied. Clears the
  // redo stack. Enforces maxDepth by discarding the oldest entry.
  void push(std::unique_ptr<Command> cmd) {
    redo_.clear();
    undo_.push_back(std::move(cmd));
    while (undo_.size() > maxDepth_) undo_.pop_front();
  }

  bool canUndo() const noexcept { return !undo_.empty(); }
  bool canRedo() const noexcept { return !redo_.empty(); }

  void undo() {
    if (undo_.empty()) return;
    auto cmd = std::move(undo_.back());
    undo_.pop_back();
    cmd->undo();
    redo_.push_back(std::move(cmd));
  }

  void redo() {
    if (redo_.empty()) return;
    auto cmd = std::move(redo_.back());
    redo_.pop_back();
    cmd->redo();
    undo_.push_back(std::move(cmd));
  }

  void clear() {
    undo_.clear();
    redo_.clear();
  }

  std::size_t undoSize() const noexcept { return undo_.size(); }
  std::size_t redoSize() const noexcept { return redo_.size(); }

 private:
  std::deque<std::unique_ptr<Command>> undo_;
  std::deque<std::unique_ptr<Command>> redo_;
  std::size_t maxDepth_;
};

}  // namespace tuxels
