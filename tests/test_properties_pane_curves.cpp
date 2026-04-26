#include <QApplication>
#include <QComboBox>
#include <QObject>

#include "core/Histogram.h"
#include "geom/Spline.h"
#include "layers/CurvesAdjustment.h"
#include "test_harness.h"
#include "ui/CurveEditor.h"
#include "ui/PropertiesPaneCurves.h"

namespace tuxels {

namespace {

// Two SplinePoint vectors are "the same" if same length and bit-equal x/y.
bool sameVec(const std::vector<SplinePoint>& a,
             const std::vector<SplinePoint>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (a[i].x != b[i].x || a[i].y != b[i].y) return false;
  }
  return true;
}

}  // namespace

TEST(curves_pane_default_state_unbound) {
  PropertiesPaneCurves pane;
  CHECK(pane.boundLayer() == nullptr);
}

TEST(curves_pane_bind_takes_snapshot) {
  PropertiesPaneCurves pane;
  CurvesAdjustment layer;
  // Tweak the layer so paramsBefore_ has something non-identity to verify.
  layer.setPoints(CurvesChannel::Composite,
                  {{0.f, 0.f}, {0.5f, 0.7f}, {1.f, 1.f}});
  pane.bind(&layer, Histogram4x256{});
  CHECK(pane.boundLayer() == &layer);
  CHECK(sameVec(pane.paramsBefore()[static_cast<int>(CurvesChannel::Composite)],
                layer.points(CurvesChannel::Composite)));
}

TEST(curves_pane_drag_release_emits_one_commit) {
  PropertiesPaneCurves pane;
  CurvesAdjustment layer;
  pane.bind(&layer, Histogram4x256{});

  int commits = 0;
  CurvesAdjustment::PointsArray capturedBefore{};
  CurvesAdjustment::PointsArray capturedAfter{};
  QObject::connect(&pane, &PropertiesPaneCurves::commitRequested,
                   [&](CurvesAdjustment*,
                       CurvesAdjustment::PointsArray b,
                       CurvesAdjustment::PointsArray a) {
                     ++commits;
                     capturedBefore = b;
                     capturedAfter = a;
                   });

  // Simulate a drag: editor's setPoints + previewChanged + interactionEnded.
  CurveEditor* ed = pane.editorForTest();
  ed->setPoints({{0.f, 0.f}, {0.5f, 0.8f}, {1.f, 1.f}});
  ed->simulatePreviewChangedForTest();
  CHECK_EQ(commits, 0);  // mid-drag — no commit yet
  ed->simulateInteractionEndedForTest();
  CHECK_EQ(commits, 1);

  // Before should be the on-bind identity (default = [(0,0),(1,1)]) for
  // the composite channel; after should reflect the new midpoint.
  const auto& beforeComp =
      capturedBefore[static_cast<int>(CurvesChannel::Composite)];
  const auto& afterComp =
      capturedAfter[static_cast<int>(CurvesChannel::Composite)];
  CHECK_EQ(static_cast<int>(beforeComp.size()), 2);
  CHECK_EQ(static_cast<int>(afterComp.size()), 3);
  CHECK_NEAR(afterComp[1].y, 0.8f, 1e-5);
}

TEST(curves_pane_release_without_change_emits_nothing) {
  PropertiesPaneCurves pane;
  CurvesAdjustment layer;
  pane.bind(&layer, Histogram4x256{});

  int commits = 0;
  QObject::connect(&pane, &PropertiesPaneCurves::commitRequested,
                   [&](CurvesAdjustment*,
                       CurvesAdjustment::PointsArray,
                       CurvesAdjustment::PointsArray) { ++commits; });

  // Release with no preceding setPoints / no diff vs snapshot.
  pane.editorForTest()->simulateInteractionEndedForTest();
  CHECK_EQ(commits, 0);
}

TEST(curves_pane_second_drag_before_equals_first_after) {
  PropertiesPaneCurves pane;
  CurvesAdjustment layer;
  pane.bind(&layer, Histogram4x256{});

  int commits = 0;
  CurvesAdjustment::PointsArray firstAfter{};
  CurvesAdjustment::PointsArray secondBefore{};
  QObject::connect(&pane, &PropertiesPaneCurves::commitRequested,
                   [&](CurvesAdjustment*,
                       CurvesAdjustment::PointsArray b,
                       CurvesAdjustment::PointsArray a) {
                     ++commits;
                     if (commits == 1) firstAfter = a;
                     if (commits == 2) secondBefore = b;
                   });

  CurveEditor* ed = pane.editorForTest();
  ed->setPoints({{0.f, 0.f}, {0.5f, 0.6f}, {1.f, 1.f}});
  ed->simulatePreviewChangedForTest();
  ed->simulateInteractionEndedForTest();
  CHECK_EQ(commits, 1);

  ed->setPoints({{0.f, 0.f}, {0.5f, 0.4f}, {1.f, 1.f}});
  ed->simulatePreviewChangedForTest();
  ed->simulateInteractionEndedForTest();
  CHECK_EQ(commits, 2);

  // Channel-by-channel sameness check (firstAfter should equal secondBefore).
  for (int ch = 0; ch < 4; ++ch) {
    CHECK(sameVec(firstAfter[ch], secondBefore[ch]));
  }
}

TEST(curves_pane_channel_switch_does_not_commit) {
  PropertiesPaneCurves pane;
  CurvesAdjustment layer;
  // Pre-edit the Red channel so switching to it surfaces non-default points.
  layer.setPoints(CurvesChannel::R,
                  {{0.f, 0.f}, {0.5f, 0.7f}, {1.f, 1.f}});
  pane.bind(&layer, Histogram4x256{});

  int commits = 0;
  QObject::connect(&pane, &PropertiesPaneCurves::commitRequested,
                   [&](CurvesAdjustment*,
                       CurvesAdjustment::PointsArray,
                       CurvesAdjustment::PointsArray) { ++commits; });

  // Switch channel — should reload editor + re-snapshot, no commit.
  pane.channelComboForTest()->setCurrentIndex(1);  // Red
  CHECK_EQ(commits, 0);
  pane.channelComboForTest()->setCurrentIndex(0);  // back to Composite
  CHECK_EQ(commits, 0);
}

TEST(curves_pane_unbind_clears_layer_pointer) {
  PropertiesPaneCurves pane;
  CurvesAdjustment layer;
  pane.bind(&layer, Histogram4x256{});
  CHECK(pane.boundLayer() == &layer);
  pane.unbind();
  CHECK(pane.boundLayer() == nullptr);
}

}  // namespace tuxels

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  return tuxels::testing::run();
}
