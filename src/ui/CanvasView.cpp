#include "ui/CanvasView.h"

#include <QBrush>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

#include "compositor/compose.h"
#include "core/Document.h"

namespace tuxels {

CanvasView::CanvasView(QWidget* parent) : QWidget(parent) {
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
  setAttribute(Qt::WA_OpaquePaintEvent, true);
}

void CanvasView::setDocument(Document* doc) {
  doc_ = doc;
  composite_ = doc_ ? TuxImage(doc_->width(), doc_->height()) : TuxImage();
  dirty_ = true;
  pan_ = {0.0, 0.0};
  zoom_ = 1.0;
  update();
}

void CanvasView::requestRecomposite() {
  dirty_ = true;
  update();
}

void CanvasView::setZoom(double z) {
  z = std::clamp(z, 0.05, 32.0);
  if (std::fabs(z - zoom_) < 1e-6) return;
  zoom_ = z;
  emit zoomChanged(zoom_);
  update();
}

void CanvasView::recomposite() {
  if (!doc_ || doc_->width() <= 0 || doc_->height() <= 0) return;
  if (composite_.width() != doc_->width() || composite_.height() != doc_->height()) {
    composite_ = TuxImage(doc_->width(), doc_->height());
  }
  compose(doc_->tree(), composite_);
  ensureCacheImage();
}

void CanvasView::ensureCacheImage() {
  const int w = composite_.width();
  const int h = composite_.height();
  if (w <= 0 || h <= 0) {
    cache_ = QImage();
    return;
  }
  if (cache_.width() != w || cache_.height() != h ||
      cache_.format() != QImage::Format_RGBA8888) {
    cache_ = QImage(w, h, QImage::Format_RGBA8888);
  }
  for (int y = 0; y < h; ++y) {
    auto* row = reinterpret_cast<uchar*>(cache_.scanLine(y));
    for (int x = 0; x < w; ++x) {
      Rgba32F p = composite_.getPixel(x, y);
      auto clamp255 = [](float v) {
        v = std::clamp(v, 0.f, 1.f);
        return static_cast<uchar>(std::lround(v * 255.f));
      };
      row[x * 4 + 0] = clamp255(p.r);
      row[x * 4 + 1] = clamp255(p.g);
      row[x * 4 + 2] = clamp255(p.b);
      row[x * 4 + 3] = clamp255(p.a);
    }
  }
}

QPointF CanvasView::canvasToWidget(QPointF p) const {
  return {p.x() * zoom_ + pan_.x(), p.y() * zoom_ + pan_.y()};
}

void CanvasView::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.fillRect(rect(), QColor(40, 40, 40));

  if (!doc_ || doc_->width() <= 0 || doc_->height() <= 0) {
    painter.setPen(Qt::lightGray);
    painter.drawText(rect(), Qt::AlignCenter,
                     tr("No document — File → New or File → Open"));
    return;
  }
  if (dirty_) {
    recomposite();
    dirty_ = false;
  }

  const int w = composite_.width();
  const int h = composite_.height();
  QPointF tl = canvasToWidget(QPointF(0, 0));
  QPointF br = canvasToWidget(QPointF(w, h));
  QRectF docRect(tl, br);

  // Checkerboard background (transparency indicator).
  {
    const int cell = 8;
    QImage checker(cell * 2, cell * 2, QImage::Format_RGB888);
    checker.fill(QColor(200, 200, 200));
    for (int y = 0; y < cell * 2; ++y) {
      for (int x = 0; x < cell * 2; ++x) {
        const bool a = (x < cell) ^ (y < cell);
        if (a) checker.setPixelColor(x, y, QColor(160, 160, 160));
      }
    }
    QBrush checkerBrush(checker);
    painter.save();
    painter.translate(docRect.topLeft());
    painter.fillRect(QRectF(0, 0, docRect.width(), docRect.height()), checkerBrush);
    painter.restore();
  }

  // Composited image.
  if (!cache_.isNull()) {
    painter.setRenderHint(QPainter::SmoothPixmapTransform, zoom_ < 1.0);
    painter.drawImage(docRect, cache_);
  }

  // Canvas border.
  painter.setPen(QPen(QColor(90, 90, 90), 1));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(docRect);
}

void CanvasView::wheelEvent(QWheelEvent* e) {
  // Ctrl+wheel → zoom centered on cursor. Plain wheel → vertical pan.
  if (e->modifiers() & Qt::ControlModifier) {
    const double factor = std::exp(e->angleDelta().y() * 0.0015);
    const QPointF cursor = e->position();
    const QPointF beforeCanvas = (cursor - pan_) / zoom_;
    setZoom(zoom_ * factor);
    pan_ = cursor - beforeCanvas * zoom_;
    update();
  } else {
    pan_ += QPointF(e->angleDelta().x() * 0.5, e->angleDelta().y() * 0.5);
    update();
  }
  e->accept();
}

void CanvasView::mousePressEvent(QMouseEvent* e) {
  if (e->button() == Qt::MiddleButton ||
      (e->button() == Qt::LeftButton && (e->modifiers() & Qt::ShiftModifier))) {
    panning_ = true;
    panStart_ = e->pos();
    panStartOffset_ = pan_;
    setCursor(Qt::ClosedHandCursor);
    e->accept();
    return;
  }
  QWidget::mousePressEvent(e);
}

void CanvasView::mouseMoveEvent(QMouseEvent* e) {
  if (panning_) {
    QPointF delta = QPointF(e->pos() - panStart_);
    pan_ = panStartOffset_ + delta;
    update();
    e->accept();
    return;
  }
  QWidget::mouseMoveEvent(e);
}

void CanvasView::mouseReleaseEvent(QMouseEvent* e) {
  if (panning_ &&
      (e->button() == Qt::MiddleButton || e->button() == Qt::LeftButton)) {
    panning_ = false;
    unsetCursor();
    e->accept();
    return;
  }
  QWidget::mouseReleaseEvent(e);
}

}  // namespace tuxels
