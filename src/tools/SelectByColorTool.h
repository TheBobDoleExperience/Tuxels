#pragma once

#include <memory>
#include <optional>
#include <string>

#include "core/SelectionMask.h"
#include "tools/ToolBase.h"

namespace tuxels {

class Document;

// Non-contiguous "select by color" — Photoshop's grouped partner to the
// magic wand. A click samples the active pixel layer's pixel color, then
// walks every pixel in that layer's backing image (layer-origin aware)
// and marks every pixel within L∞ tolerance of the seed color, regardless
// of connectivity. Commits as a SelectionCommand with the shared combine-
// mode semantics (Replace/Add/Subtract/Intersect; modifiers at press time
// win, else fall through to the persistent mode from the options row).
//
// Drag / move / release are no-ops — the gesture is a single click.
class SelectByColorTool : public ToolBase {
 public:
  SelectByColorTool() = default;

  void press(Document& doc, float x, float y, MouseButton btn) override;
  void move(Document& /*doc*/, float /*x*/, float /*y*/) override {}
  void release(Document& /*doc*/, float /*x*/, float /*y*/,
               MouseButton /*btn*/) override {}

  bool consumesShiftClick() const override { return true; }

  void setTolerance(float t) noexcept { tolerance_ = t; }
  float tolerance() const noexcept { return tolerance_; }

  void setMode(SelectionMode m) noexcept { persistentMode_ = m; }
  SelectionMode mode() const noexcept { return persistentMode_; }

  struct PendingCommit {
    std::unique_ptr<SelectionMask> before;
    std::unique_ptr<SelectionMask> after;
    std::string label;
  };
  std::optional<PendingCommit> takeCommit() {
    if (!pending_) return std::nullopt;
    auto out = std::move(*pending_);
    pending_.reset();
    return out;
  }

 private:
  static SelectionMode modeFromModifiers(int mods);

  float tolerance_ = 0.f;
  SelectionMode persistentMode_ = SelectionMode::Replace;
  std::optional<PendingCommit> pending_;
};

}  // namespace tuxels
