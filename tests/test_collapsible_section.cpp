#include <QApplication>
#include <QObject>
#include <QWidget>

#include "test_harness.h"
#include "ui/CollapsibleSection.h"

namespace tuxels {

TEST(default_state_is_expanded_and_inactive) {
  CollapsibleSection s("Brush", "B");
  CHECK(s.isExpanded());
  CHECK(!s.isActive());
}

TEST(setExpanded_toggles_state_idempotently) {
  CollapsibleSection s("Brush", "B");
  s.setExpanded(false);
  CHECK(!s.isExpanded());
  s.setExpanded(false);  // idempotent
  CHECK(!s.isExpanded());
  s.setExpanded(true);
  CHECK(s.isExpanded());
}

TEST(setActive_toggles_active_flag) {
  CollapsibleSection s("Brush", "B");
  s.setActive(true);
  CHECK(s.isActive());
  s.setActive(false);
  CHECK(!s.isActive());
}

TEST(simulateHeaderClick_emits_signal_without_collapsing) {
  CollapsibleSection s("Brush", "B");
  int headerHits = 0;
  int chevronHits = 0;
  QObject::connect(&s, &CollapsibleSection::headerClicked,
                   [&]() { ++headerHits; });
  QObject::connect(&s, &CollapsibleSection::chevronClicked,
                   [&]() { ++chevronHits; });
  s.simulateHeaderClick();
  CHECK_EQ(headerHits, 1);
  CHECK_EQ(chevronHits, 0);
  // Header click NEVER touches expansion — that's the whole UX contract.
  CHECK(s.isExpanded());
}

TEST(simulateChevronClick_toggles_expansion_and_emits_signal) {
  CollapsibleSection s("Brush", "B");
  int chevronHits = 0;
  QObject::connect(&s, &CollapsibleSection::chevronClicked,
                   [&]() { ++chevronHits; });
  CHECK(s.isExpanded());
  s.simulateChevronClick();
  CHECK_EQ(chevronHits, 1);
  CHECK(!s.isExpanded());
  s.simulateChevronClick();
  CHECK_EQ(chevronHits, 2);
  CHECK(s.isExpanded());
}

TEST(setBody_installs_widget_as_body) {
  CollapsibleSection s("Brush", "B");
  auto* body = new QWidget();
  s.setBody(body);
  CHECK_EQ(s.body(), body);
}

TEST(setBody_replaces_previous_body) {
  CollapsibleSection s("Brush", "B");
  auto* first = new QWidget();
  s.setBody(first);
  CHECK_EQ(s.body(), first);
  auto* second = new QWidget();
  s.setBody(second);
  CHECK_EQ(s.body(), second);
  // Old body is queued for deletion via deleteLater; can't safely check
  // identity here — only that the slot was swapped.
}

}  // namespace tuxels

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  return tuxels::testing::run();
}
