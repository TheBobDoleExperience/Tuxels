#include "history/MoveLayerCommand.h"

#include "core/Document.h"

namespace tuxels {

MoveLayerCommand::MoveLayerCommand(Document* doc, LayerId layerId,
                                   int beforeX, int beforeY,
                                   int afterX, int afterY)
    : doc_(doc),
      layerId_(layerId),
      beforeX_(beforeX),
      beforeY_(beforeY),
      afterX_(afterX),
      afterY_(afterY) {}

void MoveLayerCommand::undo() {
  if (!doc_) return;
  LayerBase* l = doc_->tree().findById(layerId_);
  if (!l) return;
  l->originX = beforeX_;
  l->originY = beforeY_;
}

void MoveLayerCommand::redo() {
  if (!doc_) return;
  LayerBase* l = doc_->tree().findById(layerId_);
  if (!l) return;
  l->originX = afterX_;
  l->originY = afterY_;
}

}  // namespace tuxels
