#include "ui/CanvasView.h"

#include <QBrush>
#include <QEnterEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

#include "compositor/compose.h"
#include "core/Document.h"
#include "tools/ToolBase.h"

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
  dirtyRect_ = {};
  pan_ = {0.0, 0.0};
  zoom_ = 1.0;
  update();
}

void CanvasView::requestRecomposite() {
  dirty_ = true;
  dirtyRect_ = {};  // empty + dirty_=true means "whole image"
  update();
}

void CanvasView::refreshBrushCursor() {
  if (!cursorInCanvas_) return;
  // Conservative: invalidate a rect sized for the current radius plus a
  // small pad so we cover both the old and new ring in one go.
  QRect r = brushCursorWidgetRect(cursorWidgetPos_);
  if (!r.isEmpty()) update(r.adjusted(-4, -4, 4, 4));
}

void CanvasView::requestRecomposite(Rect pixelRect) {
  if (pixelRect.isEmpty()) return;
  // If a full recomposite is already pending, don't downgrade it.
  if (dirty_ && dirtyRect_.isEmpty()) {
    update(widgetRectForPixels(pixelRect));
    return;
  }
  if (dirtyRect_.isEmpty()) {
    dirtyRect_ = pixelRect;
  } else {
    const int nx = std::min(dirtyRect_.x, pixelRect.x);
    const int ny = std::min(dirtyRect_.y, pixelRect.y);
    const int nr = std::max(dirtyRect_.right(), pixelRect.right());
    const int nb = std::max(dirtyRect_.bottom(), pixelRect.bottom());
    dirtyRect_ = {nx, ny, nr - nx, nb - ny};
  }
  dirty_ = true;
  update(widgetRectForPixels(pixelRect));
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
  ensureCacheImage(composite_.bounds());
}

void CanvasView::recompositePartial(Rect pixelRect) {
  if (!doc_ || doc_->width() <= 0 || doc_->height() <= 0) return;
  if (pixelRect.isEmpty()) return;
  if (composite_.width() != doc_->width() || composite_.height() != doc_->height()) {
    composite_ = TuxImage(doc_->width(), doc_->height());
    compose(doc_->tree(), composite_);
    ensureCacheImage(composite_.bounds());
    return;
  }
  compose(doc_->tree(), composite_, pixelRect);
  ensureCacheImage(pixelRect);
}

void CanvasView::ensureCacheImage(Rect pixelRect) {
  const int w = composite_.width();
  const int h = composite_.height();
  if (w <= 0 || h <= 0) {
    cache_ = QImage();
    return;
  }
  if (cache_.width() != w || cache_.height() != h ||
      cache_.format() != QImage::Format_RGBA8888) {
    cache_ = QImage(w, h, QImage::Format_RGBA8888);
    pixelRect = {0, 0, w, h};  // full upload on first allocation
  }
  const int x0 = std::max(0, pixelRect.x);
  const int y0 = std::max(0, pixelRect.y);
  const int x1 = std::min(w, pixelRect.right());
  const int y1 = std::min(h, pixelRect.bottom());
  if (x1 <= x0 || y1 <= y0) return;

  auto clamp255 = [](float v) {
    v = std::clamp(v, 0.f, 1.f);
    return static_cast<uchar>(std::lround(v * 255.f));
  };
  for (int y = y0; y < y1; ++y) {
    auto* row = reinterpret_cast<uchar*>(cache_.scanLine(y));
    for (int x = x0; x < x1; ++x) {
      Rgba32F p = composite_.getPixel(x, y);
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

QRect CanvasView::brushCursorWidgetRect(QPointF p) const {
  if (!tool_) return QRect();
  auto r = tool_->cursorRadiusPx();
  if (!r) return QRect();
  // Ring is in image-space pixels; scale to widget pixels via zoom.
  // Pad by 2 widget px so the outer (white) stroke + any AA fringe is
  // always inside the invalidated region.
  const double rw = (*r) * zoom_;
  const int x0 = static_cast<int>(std::floor(p.x() - rw)) - 2;
  const int y0 = static_cast<int>(std::floor(p.y() - rw)) - 2;
  const int x1 = static_cast<int>(std::ceil(p.x() + rw)) + 2;
  const int y1 = static_cast<int>(std::ceil(p.y() + rw)) + 2;
  return QRect(x0, y0, x1 - x0, y1 - y0);
}

void CanvasView::moveBrushCursorTo(QPointF p) {
  if (!tool_ || !tool_->cursorRadiusPx()) {
    cursorWidgetPos_ = p;
    return;
  }
  QRect oldR = brushCursorWidgetRect(cursorWidgetPos_);
  cursorWidgetPos_ = p;
  QRect newR = brushCursorWidgetRect(cursorWidgetPos_);
  if (!oldR.isEmpty()) update(oldR);
  if (!newR.isEmpty()) update(newR);
}

QRect CanvasView::widgetRectForPixels(Rect pixelRect) const {
  if (pixelRect.isEmpty()) return QRect();
  // Inflate by one widget pixel each side to cover antialiasing at zoom
  // boundaries. The dirty region is a hint to Qt; over-painting slightly
  // is fine.
  QPointF tl = canvasToWidget(QPointF(pixelRect.x, pixelRect.y));
  QPointF br = canvasToWidget(
      QPointF(pixelRect.right(), pixelRect.bottom()));
  const int x0 = static_cast<int>(std::floor(tl.x())) - 1;
  const int y0 = static_cast<int>(std::floor(tl.y())) - 1;
  const int x1 = static_cast<int>(std::ceil(br.x())) + 1;
  const int y1 = static_cast<int>(std::ceil(br.y())) + 1;
  return QRect(x0, y0, x1 - x0, y1 - y0);
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
    if (dirtyRect_.isEmpty()) {
      recomposite();
    } else {
      recompositePartial(dirtyRect_);
    }
    dirty_ = false;
    dirtyRect_ = {};
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

  // Brush cursor ring. Drawn as concentric black+white 1-px strokes so the
  // outline stays legible against any painted color.
  if (cursorInCanvas_ && tool_) {
    if (auto r = tool_->cursorRadiusPx()) {
      const double rw = (*r) * zoom_;
      if (rw >= 1.0) {
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(0, 0, 0, 200), 1.0));
        painter.drawEllipse(cursorWidgetPos_, rw, rw);
        painter.setPen(QPen(QColor(255, 255, 255, 220), 1.0));
        painter.drawEllipse(cursorWidgetPos_, rw + 1.0, rw + 1.0);
      }
    }
  }
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
  if (tool_ && doc_ && e->button() == Qt::LeftButton) {
    const QPointF ip = (QPointF(e->pos()) - pan_) / zoom_;
    tool_->press(*doc_, static_cast<float>(ip.x()),
                 static_cast<float>(ip.y()), MouseButton::Left);
    painting_ = true;
    moveBrushCursorTo(QPointF(e->pos()));
    const Rect dirty = tool_->takeDirtyRect();
    if (!dirty.isEmpty()) {
      requestRecomposite(dirty);
    } else {
      requestRecomposite();
    }
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
  // Track cursor pos for the brush ring even when not painting, so the
  // outline follows the mouse while hovering.
  cursorInCanvas_ = true;
  moveBrushCursorTo(QPointF(e->pos()));
  if (painting_ && tool_ && doc_) {
    const QPointF ip = (QPointF(e->pos()) - pan_) / zoom_;
    tool_->move(*doc_, static_cast<float>(ip.x()),
                static_cast<float>(ip.y()));
    const Rect dirty = tool_->takeDirtyRect();
    if (!dirty.isEmpty()) requestRecomposite(dirty);
    e->accept();
    return;
  }
  QWidget::mouseMoveEvent(e);
}

void CanvasView::enterEvent(QEnterEvent* e) {
  cursorInCanvas_ = true;
  moveBrushCursorTo(e->position());
  QWidget::enterEvent(e);
}

void CanvasView::leaveEvent(QEvent* e) {
  if (cursorInCanvas_) {
    QRect r = brushCursorWidgetRect(cursorWidgetPos_);
    cursorInCanvas_ = false;
    if (!r.isEmpty()) update(r);
  }
  QWidget::leaveEvent(e);
}

void CanvasView::mouseReleaseEvent(QMouseEvent* e) {
  if (panning_ &&
      (e->button() == Qt::MiddleButton || e->button() == Qt::LeftButton)) {
    panning_ = false;
    unsetCursor();
    e->accept();
    return;
  }
  moveBrushCursorTo(QPointF(e->pos()));
  if (painting_ && tool_ && doc_ && e->button() == Qt::LeftButton) {
    const QPointF ip = (QPointF(e->pos()) - pan_) / zoom_;
    tool_->release(*doc_, static_cast<float>(ip.x()),
                   static_cast<float>(ip.y()), MouseButton::Left);
    painting_ = false;
    const Rect dirty = tool_->takeDirtyRect();
    if (!dirty.isEmpty()) requestRecomposite(dirty);
    emit layerPainted();
    e->accept();
    return;
  }
  QWidget::mouseReleaseEvent(e);
}

}  // namespace tuxels
