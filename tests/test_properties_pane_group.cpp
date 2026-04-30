#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QObject>
#include <QSlider>

#include "compositor/BlendMode.h"
#include "layers/GroupLayer.h"
#include "test_harness.h"
#include "ui/PropertiesPaneGroup.h"

namespace tuxels {

TEST(group_pane_default_state_unbound) {
  PropertiesPaneGroup pane;
  CHECK(pane.boundLayer() == nullptr);
}

TEST(group_pane_bind_takes_snapshot) {
  PropertiesPaneGroup pane;
  GroupLayer layer;
  layer.name = "G1";
  layer.blend = BlendMode::Multiply;
  layer.opacity = 0.5f;
  layer.clipToBelow = true;
  pane.bind(&layer);
  CHECK(pane.boundLayer() == &layer);
  const auto& b = pane.paramsBefore();
  CHECK(b.name == std::string("G1"));
  CHECK(b.blend == BlendMode::Multiply);
  CHECK_NEAR(b.opacity, 0.5f, 1e-6);
  CHECK(b.clipToBelow == true);
}

TEST(group_pane_opacity_drag_release_emits_one_commit) {
  PropertiesPaneGroup pane;
  GroupLayer layer;  // default opacity = 1.0
  pane.bind(&layer);
  int commits = 0;
  GroupProperties capturedBefore{};
  GroupProperties capturedAfter{};
  QObject::connect(&pane, &PropertiesPaneGroup::commitRequested,
                   [&](GroupLayer*, GroupProperties b, GroupProperties a) {
                     ++commits;
                     capturedBefore = b;
                     capturedAfter = a;
                   });

  pane.simulateOpacityPressForTest();
  pane.opacitySpinForTest()->setValue(60.0);  // 0.6 opacity
  CHECK_EQ(commits, 0);  // mid-drag: no commit
  pane.simulateOpacityReleaseForTest();
  CHECK_EQ(commits, 1);
  CHECK_NEAR(capturedBefore.opacity, 1.0f, 1e-3);
  CHECK_NEAR(capturedAfter.opacity, 0.6f, 1e-3);
}

TEST(group_pane_release_without_change_emits_nothing) {
  PropertiesPaneGroup pane;
  GroupLayer layer;
  pane.bind(&layer);
  int commits = 0;
  QObject::connect(&pane, &PropertiesPaneGroup::commitRequested,
                   [&](GroupLayer*, GroupProperties, GroupProperties) {
                     ++commits;
                   });
  pane.simulateOpacityPressForTest();
  pane.simulateOpacityReleaseForTest();
  CHECK_EQ(commits, 0);
}

TEST(group_pane_blend_change_emits_commit) {
  PropertiesPaneGroup pane;
  GroupLayer layer;  // default blend = PassThrough (idx 0)
  pane.bind(&layer);
  int commits = 0;
  GroupProperties capturedAfter{};
  QObject::connect(&pane, &PropertiesPaneGroup::commitRequested,
                   [&](GroupLayer*, GroupProperties, GroupProperties a) {
                     ++commits;
                     capturedAfter = a;
                   });
  // kGroupBlendList: PassThrough(0), Normal(1), Dissolve(2), Darken(3),
  // Multiply(4), ...
  pane.blendComboForTest()->setCurrentIndex(4);
  CHECK_EQ(commits, 1);
  CHECK(capturedAfter.blend == BlendMode::Multiply);
}

TEST(group_pane_clip_toggle_emits_commit) {
  PropertiesPaneGroup pane;
  GroupLayer layer;
  pane.bind(&layer);
  int commits = 0;
  GroupProperties capturedAfter{};
  QObject::connect(&pane, &PropertiesPaneGroup::commitRequested,
                   [&](GroupLayer*, GroupProperties, GroupProperties a) {
                     ++commits;
                     capturedAfter = a;
                   });
  pane.clipCheckForTest()->setChecked(true);
  CHECK_EQ(commits, 1);
  CHECK(capturedAfter.clipToBelow == true);
}

TEST(group_pane_name_edit_emits_commit) {
  PropertiesPaneGroup pane;
  GroupLayer layer;
  layer.name = "Old";
  pane.bind(&layer);
  int commits = 0;
  GroupProperties capturedBefore{};
  GroupProperties capturedAfter{};
  QObject::connect(&pane, &PropertiesPaneGroup::commitRequested,
                   [&](GroupLayer*, GroupProperties b, GroupProperties a) {
                     ++commits;
                     capturedBefore = b;
                     capturedAfter = a;
                   });
  pane.nameEditForTest()->setText(QStringLiteral("New"));
  // setText alone doesn't fire editingFinished — drive the slot directly.
  pane.simulateNameEditingFinishedForTest();
  CHECK_EQ(commits, 1);
  CHECK(capturedBefore.name == std::string("Old"));
  CHECK(capturedAfter.name == std::string("New"));
}

TEST(group_pane_unbind_clears_pointer) {
  PropertiesPaneGroup pane;
  GroupLayer layer;
  pane.bind(&layer);
  CHECK(pane.boundLayer() == &layer);
  pane.unbind();
  CHECK(pane.boundLayer() == nullptr);
}

}  // namespace tuxels

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  return tuxels::testing::run();
}
