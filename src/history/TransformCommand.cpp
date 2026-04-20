#include "history/TransformCommand.h"

#include "core/Document.h"
#include "layers/PixelLayer.h"

namespace tuxels {

namespace {

PixelLayer* findPixelLayerById(Document* doc, LayerId id) {
  if (!doc) return nullptr;
  auto& tree = doc->tree();
  for (std::size_t i = 0; i < tree.size(); ++i) {
    LayerBase* l = tree.at(i);
    if (l && l->id == id) return dynamic_cast<PixelLayer*>(l);
  }
  return nullptr;
}

}  // namespace

TransformCommand::TransformCommand(Document* doc, LayerId layerId,
                                   TuxImage beforeImage, TuxImage afterImage,
                                   int beforeX, int beforeY,
                                   int afterX, int afterY,
                                   std::string label)
    : doc_(doc),
      layerId_(layerId),
      before_(std::move(beforeImage)),
      after_(std::move(afterImage)),
      beforeX_(beforeX),
      beforeY_(beforeY),
      afterX_(afterX),
      afterY_(afterY),
      label_(std::move(label)) {}

void TransformCommand::undo() {
  PixelLayer* l = findPixelLayerById(doc_, layerId_);
  if (!l) return;
  l->image = before_;
  l->originX = beforeX_;
  l->originY = beforeY_;
}

void TransformCommand::redo() {
  PixelLayer* l = findPixelLayerById(doc_, layerId_);
  if (!l) return;
  l->image = after_;
  l->originX = afterX_;
  l->originY = afterY_;
}

}  // namespace tuxels
