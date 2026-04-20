#pragma once

#include <string>

#include "core/TuxImage.h"
#include "history/Command.h"
#include "layers/LayerBase.h"

namespace tuxels {

class Document;

// Undoable origin swap produced by MoveTool. The constructor captures the
// (layerId, before, after) pair; apply/redo install `after`, undo installs
// `before`. Safe to push immediately after a live drag has already set the
// origin to `after` — apply()/redo() is idempotent.
//
// Movement can reshape a large portion of the composite (especially for
// layers that span most of the doc), so `dirtyRect()` returns empty → the
// canvas does a full recomposite. Cheap per-pixel computations already
// make that tolerable; a tight union would be a future micro-opt.
class MoveLayerCommand : public Command {
 public:
  MoveLayerCommand(Document* doc, LayerId layerId,
                   int beforeX, int beforeY,
                   int afterX, int afterY);

  void apply() override { redo(); }
  void undo() override;
  void redo() override;
  const std::string& label() const override { return label_; }

 private:
  Document* doc_;
  LayerId layerId_;
  int beforeX_;
  int beforeY_;
  int afterX_;
  int afterY_;
  std::string label_ = "Move Layer";
};

}  // namespace tuxels
