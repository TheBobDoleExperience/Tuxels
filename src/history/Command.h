#pragma once

#include <string>

#include "core/TuxImage.h"

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

  // Pixel rect whose composite is invalidated by this command. Returning
  // an empty rect is a hint to "recompose everything" — appropriate for
  // layer-structure / property changes. Paint commands can return a tight
  // bound derived from the tile snapshot.
  virtual Rect dirtyRect() const { return {}; }
};

}  // namespace tuxels
