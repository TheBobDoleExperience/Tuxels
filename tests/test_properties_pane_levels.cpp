#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QObject>
#include <QSlider>
#include <array>
#include <memory>

#include "core/Histogram.h"
#include "layers/LevelsAdjustment.h"
#include "test_harness.h"
#include "ui/PropertiesPaneLevels.h"

namespace tuxels {

namespace {

bool sameParams(const LevelsParams& a, const LevelsParams& b) {
  return a.inBlack == b.inBlack && a.inWhite == b.inWhite &&
         a.gamma == b.gamma && a.outBlack == b.outBlack &&
         a.outWhite == b.outWhite;
}

bool sameParamsArray(const std::array<LevelsParams, 4>& a,
                     const std::array<LevelsParams, 4>& b) {
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (!sameParams(a[i], b[i])) return false;
  }
  return true;
}

}  // namespace

TEST(pane_default_state_is_unbound) {
  PropertiesPaneLevels pane;
  CHECK(pane.boundLayer() == nullptr);
}

TEST(pane_bind_takes_snapshot) {
  PropertiesPaneLevels pane;
  LevelsAdjustment layer;
  // Mutate one param so the snapshot has a non-identity value to verify
  // against later.
  LevelsParams p = layer.params(LevelsChannel::Composite);
  p.gamma = 1.5f;
  layer.setParams(LevelsChannel::Composite, p);

  pane.bind(&layer, Histogram4x256{});
  CHECK(pane.boundLayer() == &layer);
  const auto& before = pane.paramsBefore();
  CHECK(sameParams(before[static_cast<int>(LevelsChannel::Composite)],
                   layer.params(LevelsChannel::Composite)));
}

TEST(pane_drag_release_emits_one_commit) {
  PropertiesPaneLevels pane;
  LevelsAdjustment layer;
  pane.bind(&layer, Histogram4x256{});

  int commits = 0;
  std::array<LevelsParams, 4> capturedBefore{};
  std::array<LevelsParams, 4> capturedAfter{};
  QObject::connect(&pane, &PropertiesPaneLevels::commitRequested,
                   [&](LevelsAdjustment*,
                       std::array<LevelsParams, 4> b,
                       std::array<LevelsParams, 4> a) {
                     ++commits;
                     capturedBefore = b;
                     capturedAfter = a;
                   });

  // Simulate a drag: press → multiple value-changes → release.
  pane.simulateSliderPressForTest();
  pane.inBlackSpinForTest()->setValue(0.10);  // chain mutates layer.
  pane.inBlackSpinForTest()->setValue(0.20);
  pane.inBlackSpinForTest()->setValue(0.30);
  CHECK_EQ(commits, 0);  // still mid-drag

  pane.simulateSliderReleaseForTest();
  CHECK_EQ(commits, 1);

  // Before should be the on-bind identity; after should reflect 0.3.
  const auto identity =
      LevelsParams{}.inBlack;  // default-constructed = 0
  CHECK_NEAR(
      capturedBefore[static_cast<int>(LevelsChannel::Composite)].inBlack,
      identity, 1e-5);
  CHECK_NEAR(
      capturedAfter[static_cast<int>(LevelsChannel::Composite)].inBlack,
      0.3f, 1e-3);
}

TEST(pane_release_without_change_emits_nothing) {
  PropertiesPaneLevels pane;
  LevelsAdjustment layer;
  pane.bind(&layer, Histogram4x256{});

  int commits = 0;
  QObject::connect(&pane, &PropertiesPaneLevels::commitRequested,
                   [&](LevelsAdjustment*,
                       std::array<LevelsParams, 4>,
                       std::array<LevelsParams, 4>) { ++commits; });

  pane.simulateSliderPressForTest();
  // No value changes inside the press/release bracket.
  pane.simulateSliderReleaseForTest();
  CHECK_EQ(commits, 0);
}

TEST(pane_second_drag_before_equals_first_after) {
  PropertiesPaneLevels pane;
  LevelsAdjustment layer;
  pane.bind(&layer, Histogram4x256{});

  int commits = 0;
  std::array<LevelsParams, 4> firstAfter{};
  std::array<LevelsParams, 4> secondBefore{};
  QObject::connect(&pane, &PropertiesPaneLevels::commitRequested,
                   [&](LevelsAdjustment*,
                       std::array<LevelsParams, 4> b,
                       std::array<LevelsParams, 4> a) {
                     ++commits;
                     if (commits == 1) firstAfter = a;
                     if (commits == 2) secondBefore = b;
                   });

  pane.simulateSliderPressForTest();
  pane.inBlackSpinForTest()->setValue(0.20);
  pane.simulateSliderReleaseForTest();
  CHECK_EQ(commits, 1);

  pane.simulateSliderPressForTest();
  pane.inBlackSpinForTest()->setValue(0.40);
  pane.simulateSliderReleaseForTest();
  CHECK_EQ(commits, 2);

  // The second drag's `before` should be the first drag's `after` — the
  // pane re-snapshots after every commit so the chain stays consistent.
  CHECK(sameParamsArray(firstAfter, secondBefore));
}

TEST(pane_unbind_clears_layer_pointer) {
  PropertiesPaneLevels pane;
  LevelsAdjustment layer;
  pane.bind(&layer, Histogram4x256{});
  CHECK(pane.boundLayer() == &layer);
  pane.unbind();
  CHECK(pane.boundLayer() == nullptr);
}

TEST(pane_preview_changed_fires_during_drag) {
  PropertiesPaneLevels pane;
  LevelsAdjustment layer;
  pane.bind(&layer, Histogram4x256{});

  int previews = 0;
  QObject::connect(&pane, &PropertiesPaneLevels::previewChanged,
                   [&]() { ++previews; });

  pane.simulateSliderPressForTest();
  pane.inBlackSpinForTest()->setValue(0.10);
  pane.inBlackSpinForTest()->setValue(0.20);
  pane.simulateSliderReleaseForTest();
  // Each value change emits one preview tick (sliders also chain to their
  // partner spin/slider via the loading_ guard, but only the first push
  // triggers a previewChanged emission per outer-edit).
  CHECK(previews >= 2);
}

}  // namespace tuxels

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  return tuxels::testing::run();
}
