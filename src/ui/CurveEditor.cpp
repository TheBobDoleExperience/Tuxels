#include "ui/CurveEditor.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <algorithm>

namespace tuxels {

CurveEditor::CurveEditor(QWidget* parent) : QWidget(parent) {
  setMinimumSize(260, 260);
  setMouseTracking(false);
}

void CurveEditor::setPoints(std::vector<SplinePoint> pts) {
  points_ = std::move(pts);
  update();
}

void CurveEditor::setHistogram(Histogram4x256 hist) {
  hist_ = hist;
  update();
}

void CurveEditor::setChannel(int ch) {
  channel_ = std::clamp(ch, 0, 3);
  update();
}

void CurveEditor::simulatePreviewChangedForTest() { emit previewChanged(); }
void CurveEditor::simulateInteractionEndedForTest() {
  emit interactionEnded();
}

void CurveEditor::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.fillRect(rect(), QColor(28, 28, 32));

  const QRectF box = gridRect();

  paintHistogram(p, box);

  p.setPen(QPen(QColor(60, 60, 70), 1.f, Qt::DashLine));
  p.drawLine(box.bottomLeft(), box.topRight());

  p.setPen(QPen(QColor(50, 50, 60), 1.f));
  for (int i = 1; i < 4; ++i) {
    const qreal f = static_cast<qreal>(i) / 4.0;
    p.drawLine(QPointF(box.left() + f * box.width(), box.top()),
               QPointF(box.left() + f * box.width(), box.bottom()));
    p.drawLine(QPointF(box.left(), box.top() + f * box.height()),
               QPointF(box.right(), box.top() + f * box.height()));
  }

  uint8_t lut[256];
  buildLut256(points_, lut);
  QColor curveColor(230, 230, 230);
  if (channel_ == 1) curveColor = QColor(220, 80, 80);
  else if (channel_ == 2) curveColor = QColor(80, 200, 80);
  else if (channel_ == 3) curveColor = QColor(80, 120, 220);
  p.setPen(QPen(curveColor, 2.f));
  QPointF prev;
  for (int i = 0; i < 256; ++i) {
    const qreal xf = static_cast<qreal>(i) / 255.0;
    const qreal yf = static_cast<qreal>(lut[i]) / 255.0;
    const QPointF pt(box.left() + xf * box.width(),
                     box.bottom() - yf * box.height());
    if (i > 0) p.drawLine(prev, pt);
    prev = pt;
  }

  p.setPen(QPen(Qt::white, 1.f));
  p.setBrush(curveColor);
  for (std::size_t i = 0; i < points_.size(); ++i) {
    const QPointF pt = toWidget(points_[i], box);
    const qreal r = (static_cast<int>(i) == dragIndex_) ? 5.5 : 4.5;
    p.drawEllipse(pt, r, r);
  }
}

void CurveEditor::mousePressEvent(QMouseEvent* e) {
  const QRectF box = gridRect();
  if (e->button() == Qt::RightButton) {
    const int hit = hitTest(e->position(), box);
    // Always keep at least 2 points so the spline has a domain.
    if (hit >= 0 && points_.size() > 2) {
      points_.erase(points_.begin() + hit);
      update();
      emit previewChanged();
      // Right-click is one-shot — no mouse release coming. Commit now.
      emit interactionEnded();
    }
    return;
  }
  if (e->button() != Qt::LeftButton) return;
  const int hit = hitTest(e->position(), box);
  if (hit >= 0) {
    dragIndex_ = hit;
    pendingInteractionCommit_ = true;
  } else {
    SplinePoint np = fromWidget(e->position(), box);
    points_.push_back(np);
    std::sort(points_.begin(), points_.end(),
              [](const SplinePoint& a, const SplinePoint& b) {
                return a.x < b.x;
              });
    for (std::size_t i = 0; i < points_.size(); ++i) {
      if (points_[i].x == np.x && points_[i].y == np.y) {
        dragIndex_ = static_cast<int>(i);
        break;
      }
    }
    pendingInteractionCommit_ = true;
    emit previewChanged();
  }
  update();
}

void CurveEditor::mouseMoveEvent(QMouseEvent* e) {
  if (dragIndex_ < 0) return;
  const QRectF box = gridRect();
  SplinePoint np = fromWidget(e->position(), box);

  if (dragIndex_ == 0) {
    np.x = 0.f;
  } else if (static_cast<std::size_t>(dragIndex_) == points_.size() - 1) {
    np.x = 1.f;
  } else {
    np.x = std::clamp(np.x,
                      points_[dragIndex_ - 1].x + 1e-4f,
                      points_[dragIndex_ + 1].x - 1e-4f);
  }
  points_[dragIndex_] = np;
  update();
  emit previewChanged();
}

void CurveEditor::mouseReleaseEvent(QMouseEvent*) {
  dragIndex_ = -1;
  update();
  if (pendingInteractionCommit_) {
    pendingInteractionCommit_ = false;
    emit interactionEnded();
  }
}

QRectF CurveEditor::gridRect() const {
  const qreal m = 8.0;
  return QRectF(rect().left() + m, rect().top() + m,
                rect().width() - 2 * m, rect().height() - 2 * m);
}

QPointF CurveEditor::toWidget(SplinePoint p, const QRectF& box) const {
  return QPointF(box.left() + p.x * box.width(),
                 box.bottom() - p.y * box.height());
}

SplinePoint CurveEditor::fromWidget(QPointF p, const QRectF& box) const {
  float x = static_cast<float>((p.x() - box.left()) / box.width());
  float y = static_cast<float>((box.bottom() - p.y()) / box.height());
  x = std::clamp(x, 0.f, 1.f);
  y = std::clamp(y, 0.f, 1.f);
  return {x, y};
}

int CurveEditor::hitTest(QPointF p, const QRectF& box) const {
  const qreal r2 = 8.0 * 8.0;
  for (std::size_t i = 0; i < points_.size(); ++i) {
    const QPointF q = toWidget(points_[i], box);
    const qreal dx = p.x() - q.x();
    const qreal dy = p.y() - q.y();
    if (dx * dx + dy * dy <= r2) return static_cast<int>(i);
  }
  return -1;
}

void CurveEditor::paintHistogram(QPainter& p, const QRectF& box) {
  if (hist_.total == 0) return;
  static const int kMap[4] = {3, 0, 1, 2};
  const int row = kMap[channel_];
  uint32_t peak = 1;
  for (int i = 0; i < 256; ++i) peak = std::max(peak, hist_.buckets[row][i]);

  p.setPen(Qt::NoPen);
  p.setBrush(QColor(60, 60, 68, 120));
  const qreal bw = box.width() / 256.0;
  for (int i = 0; i < 256; ++i) {
    const qreal bh =
        (static_cast<qreal>(hist_.buckets[row][i]) / peak) * box.height();
    p.drawRect(QRectF(box.left() + i * bw, box.bottom() - bh, bw + 1.0, bh));
  }
}

}  // namespace tuxels
