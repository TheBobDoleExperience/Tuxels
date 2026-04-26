#pragma once

#include <QWidget>
#include <vector>

#include "core/Histogram.h"
#include "geom/Spline.h"

namespace tuxels {

// 256×256 curve editor. Paints histogram backdrop + diagonal reference +
// quarter-grid + LUT curve + draggable control points. Channel determines
// the curve color (composite=white, R=red, G=green, B=blue) and which
// histogram row to render in the backdrop.
//
// Lifted out of `CurvesDialog.cpp` (M4-S3) so `PropertiesPaneCurves` can
// host it. Two signals model the full edit lifecycle:
//   - `previewChanged` fires on every visible mutation (mid-drag, add,
//     remove). The hosting pane mutates the bound layer and triggers a
//     canvas recomposite.
//   - `interactionEnded` fires once per commit-worthy gesture (mouse
//     release after a drag-or-add; right-click after a successful remove).
//     The hosting pane diffs the layer against its on-press snapshot and
//     pushes a `LayerParamsCommand` if anything changed.
class CurveEditor : public QWidget {
  Q_OBJECT

 public:
  explicit CurveEditor(QWidget* parent = nullptr);

  // Programmatic setter — does NOT emit signals. Used by the pane on
  // bind() and channel-switch.
  void setPoints(std::vector<SplinePoint> pts);
  const std::vector<SplinePoint>& points() const { return points_; }

  void setHistogram(Histogram4x256 hist);
  void setChannel(int ch);

  // Test hooks — let tests synthesize the signal sequence without
  // injecting Qt mouse events.
  void simulatePreviewChangedForTest();
  void simulateInteractionEndedForTest();

 signals:
  void previewChanged();
  void interactionEnded();

 protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent* e) override;
  void mouseMoveEvent(QMouseEvent* e) override;
  void mouseReleaseEvent(QMouseEvent* e) override;

 private:
  QRectF gridRect() const;
  QPointF toWidget(SplinePoint p, const QRectF& box) const;
  SplinePoint fromWidget(QPointF p, const QRectF& box) const;
  int hitTest(QPointF p, const QRectF& box) const;
  void paintHistogram(QPainter& p, const QRectF& box);

  Histogram4x256 hist_{};
  std::vector<SplinePoint> points_ = {{0.f, 0.f}, {1.f, 1.f}};
  int channel_ = 0;
  int dragIndex_ = -1;
  bool pendingInteractionCommit_ = false;
};

}  // namespace tuxels
