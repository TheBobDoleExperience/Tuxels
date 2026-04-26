#pragma once

#include <QWidget>

class QFrame;
class QLabel;

namespace tuxels {

// One row of the ToolsPanel accordion: a clickable header (chevron + name +
// optional shortcut hint) above an arbitrary body widget. Two distinct
// gestures: clicking the header anywhere outside the chevron emits
// `headerClicked` (the panel maps that to "activate this tool"); clicking the
// chevron itself toggles the body's visibility and emits `chevronClicked`.
//
// State is independent — sections do NOT auto-collapse when another section
// activates. The user is the only thing that can collapse a section, by
// clicking its chevron.
class CollapsibleSection : public QWidget {
  Q_OBJECT

 public:
  // `name` shows in the header bold; `shortcut` is a small hint on the right
  // edge (e.g. "B"). Empty shortcut hides the hint.
  CollapsibleSection(const QString& name, const QString& shortcut,
                     QWidget* parent = nullptr);

  // Install the body widget. Takes ownership; replaces any previous body.
  void setBody(QWidget* body);
  QWidget* body() const { return body_; }

  void setExpanded(bool expanded);
  bool isExpanded() const { return expanded_; }

  // Highlight the header to indicate this is the active tool's section. The
  // body's expanded state is unaffected.
  void setActive(bool active);
  bool isActive() const { return active_; }

  // Test hooks — drive the gestures programmatically without synthesizing Qt
  // mouse events. `simulateChevronClick` flips expansion and emits the signal,
  // matching the real eventFilter path.
  void simulateHeaderClick();
  void simulateChevronClick();

 signals:
  void headerClicked();
  void chevronClicked();

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  void updateChevronGlyph();
  void updateHeaderStyle();

  QFrame* header_ = nullptr;
  QLabel* chevron_ = nullptr;
  QLabel* nameLabel_ = nullptr;
  QLabel* shortcutLabel_ = nullptr;
  QWidget* bodyContainer_ = nullptr;
  QWidget* body_ = nullptr;
  bool expanded_ = true;
  bool active_ = false;
};

}  // namespace tuxels
