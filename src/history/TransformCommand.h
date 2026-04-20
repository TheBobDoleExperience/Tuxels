#pragma once

#include <string>

#include "core/TuxImage.h"
#include "history/Command.h"
#include "layers/LayerBase.h"

namespace tuxels {

class Document;

// Reversible "replace a pixel layer's image + origin" — the commit produced
// by Free Transform. Stores before/after TuxImages by value; the underlying
// tiles are shared_ptr-owned, so copies are cheap even for large layers,
// and later tile-COW edits on the layer don't perturb the captured
// snapshots. Finds the target layer by id, so redo after a tree reorder
// still routes correctly.
class TransformCommand : public Command {
 public:
  TransformCommand(Document* doc, LayerId layerId,
                   TuxImage beforeImage, TuxImage afterImage,
                   int beforeX, int beforeY,
                   int afterX, int afterY,
                   std::string label = "Free Transform");

  void apply() override { redo(); }
  void undo() override;
  void redo() override;
  const std::string& label() const override { return label_; }

 private:
  Document* doc_;
  LayerId layerId_;
  TuxImage before_;
  TuxImage after_;
  int beforeX_;
  int beforeY_;
  int afterX_;
  int afterY_;
  std::string label_;
};

}  // namespace tuxels
