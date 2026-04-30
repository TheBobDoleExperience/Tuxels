#pragma once

#include <memory>

#include "layers/LayerBase.h"

namespace tuxels {

class Document;

// Deep-clone a layer, allocating a fresh `LayerId` for the result and
// inheriting the source's name with " copy" appended. Dispatches on
// `LayerKind`:
//
//  - Pixel → copy-construct the TuxImage (tiles are shared_ptr-COW so the
//    copy is cheap; later edits via tileForWrite copy-on-write the
//    affected tile only) plus an optional mask.
//  - Group → recursively clone each child; the outer group's mask + props
//    are copied like a leaf.
//  - Adjustment subclasses (Levels, Curves, Hue/Saturation, Brightness/
//    Contrast) → use the type's default copy ctor (params + LUT cache) and
//    re-stamp the id.
//
// Common base fields (visible, opacity, blend, originX/Y, clipToBelow,
// mask) are copied for every kind.
//
// Unknown kinds return nullptr.
std::unique_ptr<LayerBase> cloneLayer(const LayerBase& src, Document& doc);

}  // namespace tuxels
