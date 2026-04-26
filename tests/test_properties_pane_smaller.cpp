#include <QApplication>
#include <QDoubleSpinBox>
#include <QObject>
#include <QSlider>

#include "layers/BrightnessContrast.h"
#include "layers/HueSaturation.h"
#include "test_harness.h"
#include "ui/PropertiesPaneBrightnessContrast.h"
#include "ui/PropertiesPaneHueSat.h"

namespace tuxels {

// ---------- Hue/Saturation ----------

TEST(huesat_pane_default_state_unbound) {
  PropertiesPaneHueSat pane;
  CHECK(pane.boundLayer() == nullptr);
}

TEST(huesat_pane_drag_release_emits_one_commit) {
  PropertiesPaneHueSat pane;
  HueSaturation layer;
  pane.bind(&layer);

  int commits = 0;
  HueSaturationParams capturedBefore{};
  HueSaturationParams capturedAfter{};
  QObject::connect(&pane, &PropertiesPaneHueSat::commitRequested,
                   [&](HueSaturation*, HueSaturationParams b,
                       HueSaturationParams a) {
                     ++commits;
                     capturedBefore = b;
                     capturedAfter = a;
                   });

  pane.simulateSliderPressForTest();
  pane.hueSpinForTest()->setValue(120.0);  // user dragged hue to +120°
  CHECK_EQ(commits, 0);  // mid-drag
  pane.simulateSliderReleaseForTest();
  CHECK_EQ(commits, 1);
  CHECK_NEAR(capturedBefore.hueShift, 0.0f, 1e-5);
  CHECK_NEAR(capturedAfter.hueShift, 120.0f, 1e-3);
}

TEST(huesat_pane_release_without_change_emits_nothing) {
  PropertiesPaneHueSat pane;
  HueSaturation layer;
  pane.bind(&layer);

  int commits = 0;
  QObject::connect(&pane, &PropertiesPaneHueSat::commitRequested,
                   [&](HueSaturation*, HueSaturationParams,
                       HueSaturationParams) { ++commits; });
  pane.simulateSliderPressForTest();
  pane.simulateSliderReleaseForTest();
  CHECK_EQ(commits, 0);
}

TEST(huesat_pane_unbind_clears_pointer) {
  PropertiesPaneHueSat pane;
  HueSaturation layer;
  pane.bind(&layer);
  CHECK(pane.boundLayer() == &layer);
  pane.unbind();
  CHECK(pane.boundLayer() == nullptr);
}

// ---------- Brightness/Contrast ----------

TEST(bc_pane_default_state_unbound) {
  PropertiesPaneBrightnessContrast pane;
  CHECK(pane.boundLayer() == nullptr);
}

TEST(bc_pane_drag_release_emits_one_commit) {
  PropertiesPaneBrightnessContrast pane;
  BrightnessContrast layer;
  pane.bind(&layer);

  int commits = 0;
  BrightnessContrastParams capturedAfter{};
  QObject::connect(&pane, &PropertiesPaneBrightnessContrast::commitRequested,
                   [&](BrightnessContrast*, BrightnessContrastParams,
                       BrightnessContrastParams a) {
                     ++commits;
                     capturedAfter = a;
                   });

  pane.simulateSliderPressForTest();
  pane.brightnessSpinForTest()->setValue(50.0);  // +0.5 brightness
  CHECK_EQ(commits, 0);
  pane.simulateSliderReleaseForTest();
  CHECK_EQ(commits, 1);
  CHECK_NEAR(capturedAfter.brightness, 0.5f, 1e-3);
}

TEST(bc_pane_release_without_change_emits_nothing) {
  PropertiesPaneBrightnessContrast pane;
  BrightnessContrast layer;
  pane.bind(&layer);

  int commits = 0;
  QObject::connect(&pane, &PropertiesPaneBrightnessContrast::commitRequested,
                   [&](BrightnessContrast*, BrightnessContrastParams,
                       BrightnessContrastParams) { ++commits; });
  pane.simulateSliderPressForTest();
  pane.simulateSliderReleaseForTest();
  CHECK_EQ(commits, 0);
}

TEST(bc_pane_unbind_clears_pointer) {
  PropertiesPaneBrightnessContrast pane;
  BrightnessContrast layer;
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
