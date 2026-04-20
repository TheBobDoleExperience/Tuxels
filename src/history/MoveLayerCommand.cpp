#include "history/MoveLayerCommand.h"

#include "core/Document.h"

namespace tuxels {

namespace {

LayerBase* findById(Document* doc, LayerId id) {
  if (!doc) return nullptr;
  auto& tree = doc->tree();
  for (std::size_t i = 0; i < tree.size(); ++i) {
    LayerBase* l = tree.at(i);
    if (l && l->id == id) return l;
  }
  return nullptr;
}

}  // namespace

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
  LayerBase* l = findById(doc_, layerId_);
  if (!l) return;
  l->originX = beforeX_;
  l->originY = beforeY_;
}

void MoveLayerCommand::redo() {
  LayerBase* l = findById(doc_, layerId_);
  if (!l) return;
  l->originX = afterX_;
  l->originY = afterY_;
}

}  // namespace tuxels
