#pragma once

#include <functional>
#include <string>
#include <utility>

#include "history/Command.h"

namespace tuxels {

// Generic "swap a parameter blob on an adjustment layer" command. Used by
// Levels / Curves / Hue-Sat / Brightness-Contrast dialogs to commit an edit
// to the undo stack: snapshot params on dialog-open (`before`), snapshot
// again on OK (`after`), and let the `setter` callback (usually a wrapper
// around `layer->setParams` that also rebuilds the LUT cache) move the
// values into place.
//
// The template keeps the setter signature type-checked at compile time per
// layer/params pair while letting the undo stack store the erased base.
// Header-only because every adjustment ships its own instantiation.
template <class L, class P>
class LayerParamsCommand : public Command {
 public:
  using Setter = std::function<void(L*, const P&)>;

  LayerParamsCommand(L* layer, P before, P after, Setter setter,
                     std::string label)
      : layer_(layer),
        before_(std::move(before)),
        after_(std::move(after)),
        setter_(std::move(setter)),
        label_(std::move(label)) {}

  void apply() override { setter_(layer_, after_); }
  void redo() override { setter_(layer_, after_); }
  void undo() override { setter_(layer_, before_); }
  const std::string& label() const override { return label_; }

 private:
  L* layer_;
  P before_;
  P after_;
  Setter setter_;
  std::string label_;
};

}  // namespace tuxels
