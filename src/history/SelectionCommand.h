#pragma once

#include <memory>
#include <string>
#include <utility>

#include "core/Document.h"
#include "core/SelectionMask.h"
#include "history/Command.h"

namespace tuxels {

// Wraps a selection swap on the Document as an undoable command. The
// command is pushed after the new selection has already been installed on
// the document, matching the pattern used by PaintCommand and
// LayerOpCommand elsewhere.
//
// Both `before` and `after` may be null (null = "no selection"). Clones
// are used on each undo/redo so the command keeps its stored masks
// independently of any subsequent mutation applied to the document.
class SelectionCommand : public Command {
 public:
  SelectionCommand(Document* doc,
                   std::unique_ptr<SelectionMask> before,
                   std::unique_ptr<SelectionMask> after,
                   std::string label = "Change Selection")
      : doc_(doc),
        before_(std::move(before)),
        after_(std::move(after)),
        label_(std::move(label)) {}

  void undo() override {
    doc_->setSelection(before_ ? before_->clone() : nullptr);
  }
  void redo() override {
    doc_->setSelection(after_ ? after_->clone() : nullptr);
  }
  const std::string& label() const override { return label_; }

 private:
  Document* doc_;
  std::unique_ptr<SelectionMask> before_;
  std::unique_ptr<SelectionMask> after_;
  std::string label_;
};

}  // namespace tuxels
