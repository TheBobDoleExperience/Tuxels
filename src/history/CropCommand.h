#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/SelectionMask.h"
#include "core/TuxImage.h"
#include "history/Command.h"
#include "layers/LayerBase.h"

namespace tuxels {

class Document;

// Undoable crop. Construction captures a deep-copy snapshot of the pre-crop
// document, then applies the crop in place. undo() reinstalls the snapshot;
// redo() re-applies the crop math to whatever the doc currently holds (which
// is the just-undone pre-crop state, by the linear undo/redo invariant).
//
// "Deep copy" here means tile-level clones of every PixelLayer image, every
// attached mask, and the active selection — crop is a rare operation so the
// memory spike (one full document copy on the undo stack) is acceptable.
class CropCommand : public Command {
 public:
  CropCommand(Document* doc, Rect cropRect);

  void apply() override {}  // already applied in constructor
  void undo() override;
  void redo() override;
  const std::string& label() const override { return label_; }
  // Dimensions change — force a full recompose instead of partial.
  Rect dirtyRect() const override { return {}; }

 private:
  struct LayerEntry {
    LayerId id = 0;
    int originX = 0;
    int originY = 0;
    TuxImage image;
    bool hasMask = false;
    TuxImage maskImage;
    bool maskEnabled = true;
  };
  struct Snapshot {
    int width = 0;
    int height = 0;
    std::vector<LayerEntry> layers;
    std::unique_ptr<SelectionMask> selection;
  };

  static Snapshot captureDeep(const Document& doc);
  static void installDeep(Document& doc, const Snapshot& snap);
  static void applyCropInPlace(Document& doc, Rect cropRect);

  Document* doc_;
  Rect cropRect_;
  Snapshot before_;
  std::string label_ = "Crop";
};

}  // namespace tuxels
