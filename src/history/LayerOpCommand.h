#pragma once

#include <functional>
#include <string>
#include <utility>

#include "history/Command.h"

namespace tuxels {

// Generic layer-structure/property command parametrized by lambdas.
// The `do` lambda runs on apply() and redo(); `undo` lambda on undo().
// Use this when the caller can express both directions as closures that
// capture the state they need (e.g. via shared_ptr stash for deleted
// layers).
class LayerOpCommand : public Command {
 public:
  using Fn = std::function<void()>;

  LayerOpCommand(std::string label, Fn doIt, Fn undoIt)
      : label_(std::move(label)),
        do_(std::move(doIt)),
        undo_(std::move(undoIt)) {}

  void apply() override { do_(); }
  void undo() override { undo_(); }
  void redo() override { do_(); }
  const std::string& label() const override { return label_; }

 private:
  std::string label_;
  Fn do_;
  Fn undo_;
};

}  // namespace tuxels
