#pragma once

#include <string>

namespace tuxels {

// Abstract base for reversible editor actions. apply() is the first-time
// execution path (commit); redo() is the replay path; undo() reverts.
// For simple commands apply()==redo(), so a default is provided.
class Command {
 public:
  virtual ~Command() = default;
  virtual void apply() { redo(); }
  virtual void undo() = 0;
  virtual void redo() = 0;
  virtual const std::string& label() const = 0;
};

}  // namespace tuxels
