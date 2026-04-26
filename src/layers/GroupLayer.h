#pragma once

#include <memory>
#include <vector>

#include "layers/LayerBase.h"

namespace tuxels {

// A layer that owns other layers as children — the PS "Group" / "Folder"
// concept. Has its own visibility/opacity/blend/mask/clipToBelow (inherited
// from LayerBase) plus an `isExpanded` UI flag that's persisted in `.txl`
// (matches PS).
//
// Default blend is `BlendMode::PassThrough` — children composite as if the
// group were transparent (adjustments inside leak out to layers below).
// When set to any other blend mode the group is **isolated**: children
// composite into a private buffer first, then the buffer composites back
// into the parent stack as a single unit (adjustments inside don't leak).
//
// `kind()` returns `Group`; `renderTile` returns false because the compose
// recursion handles groups directly (it never calls `renderTile` on a group).
// Mirrors `AdjustmentLayer` in shape — both kinds contribute no pixels of
// their own.
class GroupLayer : public LayerBase {
 public:
  GroupLayer() {
    blend = BlendMode::PassThrough;
  }

  std::vector<std::unique_ptr<LayerBase>> children;

  // Per-PS, group expand/collapse persists in the document (PSD section
  // divider records carry the same flag). Compose is unaffected — collapse
  // is purely a UI affordance.
  bool isExpanded = true;

  LayerKind kind() const final { return LayerKind::Group; }

  // Compose recursion handles groups directly via `composeChildren`; this
  // method should never be reached during compose. Returning false (rather
  // than asserting) keeps the LayerBase contract that every layer is
  // callable. Mirrors `AdjustmentLayer::renderTile`.
  bool renderTile(TileCoord /*tc*/, Rgba32F* /*out*/) const final {
    return false;
  }
};

}  // namespace tuxels
