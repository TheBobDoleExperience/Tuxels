#include "ui/CurvesDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>

#include "geom/Spline.h"

namespace tuxels {

// Custom widget: 256×256 curve grid. Paints the active channel's histogram
// backdrop (faded), the resulting LUT, and draggable control points.
// Emits `pointsChanged` whenever the caller can observe a new LUT — that
// signal drives the dialog's live preview + LayerParamsCommand snapshot.
class CurveEditor : public QWidget {
  Q_OBJECT
 public:
  CurveEditor(Histogram4x256 hist, QWidget* parent = nullptr)
      : QWidget(parent), hist_(hist) {
    setMinimumSize(260, 260);
    setMouseTracking(false);
  }

  void setPoints(std::vector<SplinePoint> pts) {
    points_ = std::move(pts);
    update();
  }
  const std::vector<SplinePoint>& points() const { return points_; }

  void setChannel(int ch) {
    channel_ = std::clamp(ch, 0, 3);
    update();
  }

 signals:
  void pointsChanged();

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.fillRect(rect(), QColor(28, 28, 32));

    const QRectF box = gridRect();

    // Histogram backdrop (faded).
    paintHistogram(p, box);

    // Diagonal reference line.
    p.setPen(QPen(QColor(60, 60, 70), 1.f, Qt::DashLine));
    p.drawLine(box.bottomLeft(), box.topRight());

    // Grid (quarter divisions).
    p.setPen(QPen(QColor(50, 50, 60), 1.f));
    for (int i = 1; i < 4; ++i) {
      const qreal f = static_cast<qreal>(i) / 4.0;
      p.drawLine(QPointF(box.left() + f * box.width(), box.top()),
                 QPointF(box.left() + f * box.width(), box.bottom()));
      p.drawLine(QPointF(box.left(), box.top() + f * box.height()),
                 QPointF(box.right(), box.top() + f * box.height()));
    }

    // LUT curve.
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

    // Control point dots.
    p.setPen(QPen(Qt::white, 1.f));
    p.setBrush(curveColor);
    for (std::size_t i = 0; i < points_.size(); ++i) {
      const QPointF pt = toWidget(points_[i], box);
      const qreal r = (static_cast<int>(i) == dragIndex_) ? 5.5 : 4.5;
      p.drawEllipse(pt, r, r);
    }
  }

  void mousePressEvent(QMouseEvent* e) override {
    const QRectF box = gridRect();
    if (e->button() == Qt::RightButton) {
      const int hit = hitTest(e->position(), box);
      // Always keep at least 2 points so the spline has a domain.
      if (hit >= 0 && points_.size() > 2) {
        points_.erase(points_.begin() + hit);
        update();
        emit pointsChanged();
      }
      return;
    }
    if (e->button() != Qt::LeftButton) return;
    const int hit = hitTest(e->position(), box);
    if (hit >= 0) {
      dragIndex_ = hit;
    } else {
      // Add a new control point at the cursor (clamped to grid).
      SplinePoint np = fromWidget(e->position(), box);
      points_.push_back(np);
      std::sort(points_.begin(), points_.end(),
                [](const SplinePoint& a, const SplinePoint& b) {
                  return a.x < b.x;
                });
      // Find the new index so the subsequent drag tracks it.
      for (std::size_t i = 0; i < points_.size(); ++i) {
        if (points_[i].x == np.x && points_[i].y == np.y) {
          dragIndex_ = static_cast<int>(i);
          break;
        }
      }
      emit pointsChanged();
    }
    update();
  }

  void mouseMoveEvent(QMouseEvent* e) override {
    if (dragIndex_ < 0) return;
    const QRectF box = gridRect();
    SplinePoint np = fromWidget(e->position(), box);

    // Clamp x between neighbours so the point list stays sorted without a
    // re-sort. Endpoints (index 0 / last) are constrained to their grid edge.
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
    emit pointsChanged();
  }

  void mouseReleaseEvent(QMouseEvent*) override {
    dragIndex_ = -1;
    update();
  }

 private:
  QRectF gridRect() const {
    const qreal m = 8.0;
    return QRectF(rect().left() + m, rect().top() + m,
                  rect().width() - 2 * m, rect().height() - 2 * m);
  }

  QPointF toWidget(SplinePoint p, const QRectF& box) const {
    return QPointF(box.left() + p.x * box.width(),
                   box.bottom() - p.y * box.height());
  }

  SplinePoint fromWidget(QPointF p, const QRectF& box) const {
    float x = static_cast<float>((p.x() - box.left()) / box.width());
    float y = static_cast<float>((box.bottom() - p.y()) / box.height());
    x = std::clamp(x, 0.f, 1.f);
    y = std::clamp(y, 0.f, 1.f);
    return {x, y};
  }

  int hitTest(QPointF p, const QRectF& box) const {
    const qreal r2 = 8.0 * 8.0;
    for (std::size_t i = 0; i < points_.size(); ++i) {
      const QPointF q = toWidget(points_[i], box);
      const qreal dx = p.x() - q.x();
      const qreal dy = p.y() - q.y();
      if (dx * dx + dy * dy <= r2) return static_cast<int>(i);
    }
    return -1;
  }

  void paintHistogram(QPainter& p, const QRectF& box) {
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

  Histogram4x256 hist_;
  std::vector<SplinePoint> points_ = {{0.f, 0.f}, {1.f, 1.f}};
  int channel_ = 0;
  int dragIndex_ = -1;
};

}  // namespace tuxels

#include "CurvesDialog.moc"

namespace tuxels {

CurvesDialog::CurvesDialog(CurvesAdjustment* layer, Histogram4x256 hist,
                           QWidget* parent)
    : QDialog(parent), layer_(layer), before_(layer->allPoints()) {
  setWindowTitle(tr("Curves"));
  setModal(true);

  auto* layout = new QVBoxLayout(this);

  channelCombo_ = new QComboBox(this);
  channelCombo_->addItem(tr("Composite"));
  channelCombo_->addItem(tr("Red"));
  channelCombo_->addItem(tr("Green"));
  channelCombo_->addItem(tr("Blue"));
  layout->addWidget(channelCombo_);

  editor_ = new CurveEditor(hist, this);
  editor_->setPoints(layer_->points(CurvesChannel::Composite));
  editor_->setChannel(0);
  layout->addWidget(editor_, 1);

  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttons);

  connect(channelCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &CurvesDialog::onChannelChanged);
  connect(editor_, &CurveEditor::pointsChanged, this,
          &CurvesDialog::onEditorPointsChanged);
}

void CurvesDialog::onChannelChanged(int idx) {
  activeChannel_ = static_cast<CurvesChannel>(idx);
  editor_->setChannel(idx);
  editor_->setPoints(layer_->points(activeChannel_));
}

void CurvesDialog::onEditorPointsChanged() {
  layer_->setPoints(activeChannel_, editor_->points());
  emit previewChanged();
}

void CurvesDialog::reject() {
  layer_->setAllPoints(before_);
  emit previewChanged();
  QDialog::reject();
}

}  // namespace tuxels
