#include "ui/CanvasView.h"

#include <QBrush>
#include <QEnterEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QPolygonF>
#include <QTimer>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

#include "compositor/compose.h"
#include "core/Document.h"
#include "core/SelectionMask.h"
#include "tools/ToolBase.h"
#include "tools/TransformTool.h"

namespace tuxels {

CanvasView::CanvasView(QWidget* parent) : QWidget(parent) {
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
  setAttribute(Qt::WA_OpaquePaintEvent, true);

  antsTimer_ = new QTimer(this);
  antsTimer_->setInterval(100);  // 10 Hz phase tick
  connect(antsTimer_, &QTimer::timeout, this, [this]() {
    if (selectionSegments_.empty()) return;
    antsPhase_ = (antsPhase_ + 1) % 8;
    QRect r = selectionWidgetRect();
    if (!r.isEmpty()) update(r);
  });
}

namespace {

// Shared pixmap scaffolding. 32×32 logical, 2× device pixel ratio so the
// art stays crisp on hi-DPI. Black stroke + white outline keeps the cursor
// legible over both light and dark image content (same trick the brush
// ring uses). Hotspot is in logical coords.
QPixmap makeCursorPixmap(std::function<void(QPainter&)> draw) {
  constexpr int S = 32;
  QPixmap pm(S * 2, S * 2);
  pm.setDevicePixelRatio(2.0);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  draw(p);
  return pm;
}

QCursor makeBucketCursor() {
  QPixmap pm = makeCursorPixmap([](QPainter& p) {
    // Body: angled trapezoidal bucket. Points at the drop spot in the
    // lower-left so the hotspot sits where the paint comes out.
    QPolygonF body({
        QPointF(8.0, 6.0),   // rim-left
        QPointF(24.0, 10.0), // rim-right
        QPointF(21.0, 24.0), // base-right
        QPointF(11.0, 22.0), // base-left
    });
    p.setPen(QPen(Qt::white, 3.0, Qt::SolidLine, Qt::RoundCap,
                  Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawPolygon(body);
    p.setPen(QPen(Qt::black, 1.5, Qt::SolidLine, Qt::RoundCap,
                  Qt::RoundJoin));
    p.setBrush(QColor(240, 240, 240));
    p.drawPolygon(body);
    // Rim ellipse so it reads as a bucket opening.
    p.setBrush(QColor(210, 210, 210));
    p.drawEllipse(QPointF(16.0, 8.0), 8.0, 2.0);
    // Handle arcing above the rim.
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(Qt::black, 1.2));
    QPainterPath handle;
    handle.moveTo(9.0, 7.0);
    handle.quadTo(16.0, -1.0, 23.0, 9.0);
    p.drawPath(handle);
    // Paint drop at hotspot (4, 28) — angled tear.
    QPainterPath drop;
    drop.moveTo(4.0, 28.0);
    drop.quadTo(1.0, 24.0, 6.0, 22.0);
    drop.quadTo(9.0, 24.0, 4.0, 28.0);
    p.setPen(QPen(Qt::white, 2.5));
    p.drawPath(drop);
    p.setPen(QPen(Qt::black, 1.0));
    p.setBrush(QColor(50, 120, 220));
    p.drawPath(drop);
  });
  return QCursor(pm, 4, 28);
}

QCursor makeWandCursor() {
  QPixmap pm = makeCursorPixmap([](QPainter& p) {
    // Wand shaft running from lower-left to upper-right. Star burst at
    // the tip; tip pixel is the hotspot so the wand "picks" exactly where
    // the user clicks.
    QPointF tail(4.0, 28.0);
    QPointF tip(22.0, 10.0);
    // Shaft white underlay then black.
    p.setPen(QPen(Qt::white, 4.0, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(tail, tip);
    p.setPen(QPen(Qt::black, 2.0, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(tail, tip);
    // Tip diamond: four-point star rotated 45°.
    QPolygonF star({
        QPointF(22.0, 4.0),
        QPointF(25.0, 10.0),
        QPointF(22.0, 16.0),
        QPointF(19.0, 10.0),
    });
    p.setPen(QPen(Qt::white, 2.0));
    p.setBrush(Qt::NoBrush);
    p.drawPolygon(star);
    p.setPen(QPen(Qt::black, 1.0));
    p.setBrush(QColor(255, 230, 120));
    p.drawPolygon(star);
    // Two short sparkle rays orthogonal to the shaft.
    p.setPen(QPen(Qt::white, 2.0, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(26.0, 6.0), QPointF(28.0, 4.0));
    p.drawLine(QPointF(18.0, 14.0), QPointF(16.0, 16.0));
    p.setPen(QPen(Qt::black, 1.0, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(26.0, 6.0), QPointF(28.0, 4.0));
    p.drawLine(QPointF(18.0, 14.0), QPointF(16.0, 16.0));
  });
  return QCursor(pm, 22, 10);
}

QCursor makeCropCursor() {
  QPixmap pm = makeCursorPixmap([](QPainter& p) {
    // Two L-brackets at opposing corners make the classic crop cursor.
    // Hotspot is the center (16, 16) — matches Photoshop, where clicks
    // anchor the crop-box corner to the cursor point.
    auto drawL = [&](QPointF origin, QPointF armA, QPointF armB,
                     qreal width) {
      p.setPen(QPen(Qt::white, width + 2.0, Qt::SolidLine, Qt::SquareCap));
      p.drawLine(origin, armA);
      p.drawLine(origin, armB);
      p.setPen(QPen(Qt::black, width, Qt::SolidLine, Qt::SquareCap));
      p.drawLine(origin, armA);
      p.drawLine(origin, armB);
    };
    // Upper-left bracket.
    drawL(QPointF(6.0, 6.0), QPointF(14.0, 6.0), QPointF(6.0, 14.0), 2.0);
    // Lower-right bracket.
    drawL(QPointF(26.0, 26.0), QPointF(18.0, 26.0),
          QPointF(26.0, 18.0), 2.0);
    // Center crosshair so the hotspot is visually obvious.
    p.setPen(QPen(Qt::white, 3.0, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(16.0, 13.0), QPointF(16.0, 19.0));
    p.drawLine(QPointF(13.0, 16.0), QPointF(19.0, 16.0));
    p.setPen(QPen(Qt::black, 1.2, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(16.0, 13.0), QPointF(16.0, 19.0));
    p.drawLine(QPointF(13.0, 16.0), QPointF(19.0, 16.0));
  });
  return QCursor(pm, 16, 16);
}

}  // namespace

QCursor CanvasView::cursorForTool(ToolId id) {
  switch (id) {
    case ToolId::Brush:
      // The ring overlay already communicates position + radius; a plain
      // arrow keeps the image readable under the ring.
      return QCursor(Qt::ArrowCursor);
    case ToolId::Marquee:
      return QCursor(Qt::CrossCursor);
    case ToolId::Bucket:
      return makeBucketCursor();
    case ToolId::MagicWand:
      return makeWandCursor();
    case ToolId::Crop:
      return makeCropCursor();
    case ToolId::Move:
      return QCursor(Qt::SizeAllCursor);
    case ToolId::Transform:
      // Plain arrow — the bbox + corner dots already communicate grab
      // points, and the cursor shape changes would need to be region-aware
      // (which we can add later with hover-test in the paint path).
      return QCursor(Qt::ArrowCursor);
    case ToolId::Lasso:
    case ToolId::PolyLasso:
      // Crosshair matches the marquee idiom and keeps vertex placement
      // precise at any zoom.
      return QCursor(Qt::CrossCursor);
    case ToolId::SelectByColor:
      // Reuse the wand pixmap — Photoshop groups them and users expect
      // the same cursor shape.
      return makeWandCursor();
  }
  return QCursor(Qt::ArrowCursor);
}

void CanvasView::setToolCursor(const QCursor& c) {
  toolCursor_ = c;
  // Don't stomp the closed-hand cursor mid-pan; restored on pan release.
  if (!panning_) setCursor(toolCursor_);
}

int CanvasView::translateModifiers(int qt) {
  int m = Mod::None;
  if (qt & Qt::ShiftModifier) m |= Mod::Shift;
  if (qt & Qt::AltModifier)   m |= Mod::Alt;
  if (qt & Qt::ControlModifier) m |= Mod::Ctrl;
  return m;
}

void CanvasView::setDocument(Document* doc) {
  doc_ = doc;
  composite_ = doc_ ? TuxImage(doc_->width(), doc_->height()) : TuxImage();
  dirty_ = true;
  dirtyRect_ = {};
  pan_ = {0.0, 0.0};
  zoom_ = 1.0;
  selectionSegments_.clear();
  selectionBounds_ = {};
  lastSelection_ = nullptr;
  antsTimer_->stop();
  update();
}

void CanvasView::requestRecomposite() {
  dirty_ = true;
  dirtyRect_ = {};  // empty + dirty_=true means "whole image"
  update();
}

void CanvasView::refreshSelectionOverlay() {
  rebuildSelectionSegments();
  if (selectionSegments_.empty()) {
    antsTimer_->stop();
  } else if (!antsTimer_->isActive()) {
    antsTimer_->start();
  }
  update();
}

void CanvasView::rebuildSelectionSegments() {
  selectionSegments_.clear();
  selectionBounds_ = {};
  if (!doc_) {
    lastSelection_ = nullptr;
    return;
  }
  const SelectionMask* sel = doc_->selection();
  lastSelection_ = sel;
  if (!sel) return;

  const Rect b = sel->boundsOfSelected(0.5f);
  if (b.isEmpty()) return;
  selectionBounds_ = b;

  const int x0 = b.x;
  const int y0 = b.y;
  const int x1 = b.right();
  const int y1 = b.bottom();
  // One cursor per row-relative offset we probe (current, up, down). Each
  // walks sequentially in x so the tile pointer cache stays hot — per-tile
  // hash lookup instead of per-pixel. Huge win on large wand selections.
  TileRowCursor cur(sel->image()), up(sel->image()), dn(sel->image());
  selectionSegments_.reserve(256);
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      if (cur.sampleR(x, y) <= 0.5f) continue;
      if (cur.sampleR(x - 1, y) <= 0.5f) {
        selectionSegments_.emplace_back(QPointF(x, y), QPointF(x, y + 1));
      }
      if (cur.sampleR(x + 1, y) <= 0.5f) {
        selectionSegments_.emplace_back(QPointF(x + 1, y),
                                        QPointF(x + 1, y + 1));
      }
      if (up.sampleR(x, y - 1) <= 0.5f) {
        selectionSegments_.emplace_back(QPointF(x, y), QPointF(x + 1, y));
      }
      if (dn.sampleR(x, y + 1) <= 0.5f) {
        selectionSegments_.emplace_back(QPointF(x, y + 1),
                                        QPointF(x + 1, y + 1));
      }
    }
  }
}

QRect CanvasView::selectionWidgetRect() const {
  if (selectionBounds_.isEmpty()) return QRect();
  return widgetRectForPixels(selectionBounds_).adjusted(-2, -2, 2, 2);
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
  LayerOverride ov;
  const LayerOverride* ovp = nullptr;
  if (transformTool_) {
    auto o = transformTool_->overlay();
    if (o.active) { ov = o.layer; ovp = &ov; }
  }
  compose(doc_->tree(), composite_, ovp);
  ensureCacheImage(composite_.bounds());
}

void CanvasView::recompositePartial(Rect pixelRect) {
  if (!doc_ || doc_->width() <= 0 || doc_->height() <= 0) return;
  if (pixelRect.isEmpty()) return;
  LayerOverride ov;
  const LayerOverride* ovp = nullptr;
  if (transformTool_) {
    auto o = transformTool_->overlay();
    if (o.active) { ov = o.layer; ovp = &ov; }
  }
  if (composite_.width() != doc_->width() || composite_.height() != doc_->height()) {
    composite_ = TuxImage(doc_->width(), doc_->height());
    compose(doc_->tree(), composite_, ovp);
    ensureCacheImage(composite_.bounds());
    return;
  }
  compose(doc_->tree(), composite_, pixelRect, ovp);
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
  // Scanline-cached read: one hash lookup per tile boundary rather than
  // per pixel. Makes large post-fill / post-crop re-uploads snappy.
  TileRowCursor cursor(composite_);
  for (int y = y0; y < y1; ++y) {
    auto* row = reinterpret_cast<uchar*>(cache_.scanLine(y));
    for (int x = x0; x < x1; ++x) {
      Rgba32F p = cursor.at(x, y);
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

QRect CanvasView::widgetRectForPath(const std::vector<Point2f>& path) const {
  if (path.size() < 2) return QRect();
  float minX = path[0].x, maxX = path[0].x;
  float minY = path[0].y, maxY = path[0].y;
  for (const auto& p : path) {
    minX = std::min(minX, p.x);
    maxX = std::max(maxX, p.x);
    minY = std::min(minY, p.y);
    maxY = std::max(maxY, p.y);
  }
  QPointF tl = canvasToWidget(QPointF(minX, minY));
  QPointF br = canvasToWidget(QPointF(maxX, maxY));
  // Pad by 2 widget pixels for the dashed pen stroke; same trick
  // `widgetRectForPixels` uses for the rubber-band rectangle.
  const int x0 = static_cast<int>(std::floor(tl.x())) - 2;
  const int y0 = static_cast<int>(std::floor(tl.y())) - 2;
  const int x1 = static_cast<int>(std::ceil(br.x())) + 2;
  const int y1 = static_cast<int>(std::ceil(br.y())) + 2;
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

  // Detect an out-of-band selection change (menu ops, undo/redo) and
  // refresh the cached ants overlay before painting.
  if (doc_->selection() != lastSelection_) {
    rebuildSelectionSegments();
    if (selectionSegments_.empty()) {
      antsTimer_->stop();
    } else if (!antsTimer_->isActive()) {
      antsTimer_->start();
    }
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

  // Marching-ants overlay for the active selection. Black solid underlay
  // plus a white dashed stroke whose dashOffset is animated by antsTimer_
  // so the ants appear to crawl along the boundary.
  if (!selectionSegments_.empty()) {
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.save();
    painter.translate(pan_);
    painter.scale(zoom_, zoom_);
    QPen black(QColor(0, 0, 0, 220), 1.0);
    black.setCosmetic(true);
    painter.setPen(black);
    painter.drawLines(selectionSegments_.data(),
                      static_cast<int>(selectionSegments_.size()));
    QPen white(QColor(255, 255, 255, 240), 1.0);
    white.setCosmetic(true);
    white.setStyle(Qt::CustomDashLine);
    white.setDashPattern({4.0, 4.0});
    white.setDashOffset(static_cast<double>(antsPhase_));
    painter.setPen(white);
    painter.drawLines(selectionSegments_.data(),
                      static_cast<int>(selectionSegments_.size()));
    painter.restore();
  }

  // Rubber-band rectangle while a tool with a live-rect drag is in progress
  // (marquee and crop both surface theirs via ToolBase::liveRect()).
  if (tool_) {
    if (auto r = tool_->liveRect()) {
      painter.setRenderHint(QPainter::Antialiasing, false);
      QPointF tl = canvasToWidget(QPointF(r->x, r->y));
      QPointF br2 = canvasToWidget(QPointF(r->right(), r->bottom()));
      QRectF rubber(tl, br2);
      QPen black(QColor(0, 0, 0, 220), 1.0);
      painter.setPen(black);
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(rubber);
      QPen white(QColor(255, 255, 255, 240), 1.0);
      white.setStyle(Qt::DashLine);
      painter.setPen(white);
      painter.drawRect(rubber);
    }
  }

  // Live lasso / polygonal-lasso polyline. Drawn as an open polyline
  // (close segment is implicit on commit) with the same black-underlay +
  // white-dashed-overlay treatment as the marching ants so the in-progress
  // outline stays legible over any image content.
  if (tool_) {
    if (auto path = tool_->livePath()) {
      if (path->size() >= 2) {
        painter.setRenderHint(QPainter::Antialiasing, false);
        QPolygonF poly;
        poly.reserve(static_cast<int>(path->size()));
        for (const auto& p : *path) {
          poly.append(canvasToWidget(QPointF(p.x, p.y)));
        }
        painter.setBrush(Qt::NoBrush);
        QPen black(QColor(0, 0, 0, 220), 1.0);
        painter.setPen(black);
        painter.drawPolyline(poly);
        QPen white(QColor(255, 255, 255, 240), 1.0);
        white.setStyle(Qt::DashLine);
        painter.setPen(white);
        painter.drawPolyline(poly);
      }
    }
  }

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

  // Free Transform overlay: bbox quad + 4 corner handles in widget-space.
  // Drawn after everything else so the handles stay on top of the preview.
  if (transformTool_) {
    auto ov = transformTool_->overlay();
    if (ov.active) {
      painter.setRenderHint(QPainter::Antialiasing, true);
      QPointF c[4];
      for (int i = 0; i < 4; ++i) {
        c[i] = canvasToWidget(QPointF(ov.corners[i][0], ov.corners[i][1]));
      }
      QPolygonF poly({c[0], c[1], c[2], c[3]});
      QPen black(QColor(0, 0, 0, 220), 1.0);
      painter.setBrush(Qt::NoBrush);
      painter.setPen(black);
      painter.drawPolygon(poly);
      QPen white(QColor(255, 255, 255, 240), 1.0);
      white.setStyle(Qt::DashLine);
      painter.setPen(white);
      painter.drawPolygon(poly);
      // Corner handles: 6×6 filled squares with a 1-px black outline.
      const qreal hs = 3.0;
      for (int i = 0; i < 4; ++i) {
        QRectF h(c[i].x() - hs, c[i].y() - hs, hs * 2.0, hs * 2.0);
        painter.setPen(QPen(QColor(0, 0, 0, 220), 1.0));
        painter.setBrush(QColor(255, 255, 255, 240));
        painter.drawRect(h);
      }
      // Pivot: a small circle with a crosshair through it, always drawn
      // last so it sits on top of the bbox even when the pivot is on an
      // edge or corner.
      const QPointF pv = canvasToWidget(QPointF(ov.pivot[0], ov.pivot[1]));
      const qreal pr = 5.0;
      painter.setPen(QPen(QColor(0, 0, 0, 220), 1.0));
      painter.setBrush(QColor(255, 255, 255, 240));
      painter.drawEllipse(pv, pr, pr);
      painter.setPen(QPen(QColor(0, 0, 0, 220), 1.0));
      painter.drawLine(QPointF(pv.x() - pr - 2.0, pv.y()),
                       QPointF(pv.x() + pr + 2.0, pv.y()));
      painter.drawLine(QPointF(pv.x(), pv.y() - pr - 2.0),
                       QPointF(pv.x(), pv.y() + pr + 2.0));
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
  const bool shiftLeft = (e->button() == Qt::LeftButton &&
                          (e->modifiers() & Qt::ShiftModifier));
  const bool toolClaimsShift = tool_ && tool_->consumesShiftClick();
  if (e->button() == Qt::MiddleButton ||
      (shiftLeft && !toolClaimsShift)) {
    panning_ = true;
    panStart_ = e->pos();
    panStartOffset_ = pan_;
    setCursor(Qt::ClosedHandCursor);
    e->accept();
    return;
  }
  if (tool_ && doc_ && e->button() == Qt::LeftButton) {
    tool_->setModifiers(translateModifiers(e->modifiers()));
    const QPointF ip = (QPointF(e->pos()) - pan_) / zoom_;
    std::optional<std::vector<Point2f>> prePath = tool_->livePath();
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
    // Also invalidate the rubber-band rect region if the tool now has a
    // live rect (press may set a 1-px rect that needs drawing).
    if (auto r = tool_->liveRect()) update(widgetRectForPixels(*r));
    // Poly-lasso close-on-press may have torn down the in-progress path;
    // repaint wherever the old path lived. Symmetrically repaint the new
    // path region in case the press extended the polyline.
    if (prePath) update(widgetRectForPath(*prePath));
    if (auto p = tool_->livePath()) update(widgetRectForPath(*p));
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
    // Capture the rubber-band rect AND polyline BEFORE the move so we can
    // invalidate their old widget-space extents; then compute new ones.
    std::optional<Rect> preRect = tool_->liveRect();
    std::optional<std::vector<Point2f>> prePath = tool_->livePath();

    // Re-read modifiers on each move so Shift held mid-drag reaches the
    // tool (transform aspect-lock, rotation snap, …).
    tool_->setModifiers(translateModifiers(e->modifiers()));
    const QPointF ip = (QPointF(e->pos()) - pan_) / zoom_;
    tool_->move(*doc_, static_cast<float>(ip.x()),
                static_cast<float>(ip.y()));
    const Rect dirty = tool_->takeDirtyRect();
    if (!dirty.isEmpty()) requestRecomposite(dirty);

    if (preRect) update(widgetRectForPixels(*preRect));
    if (auto r = tool_->liveRect()) update(widgetRectForPixels(*r));
    if (prePath) update(widgetRectForPath(*prePath));
    if (auto p = tool_->livePath()) update(widgetRectForPath(*p));
    e->accept();
    return;
  }
  // Not in a drag — still forward to hover() so the polygonal lasso can
  // track the cursor for its rubber-band last edge. Repaint the old and
  // new polyline bboxes so the trailing line follows the mouse without a
  // full-canvas invalidate.
  if (tool_ && doc_) {
    std::optional<std::vector<Point2f>> prePath = tool_->livePath();
    const QPointF ip = (QPointF(e->pos()) - pan_) / zoom_;
    tool_->hover(*doc_, static_cast<float>(ip.x()),
                 static_cast<float>(ip.y()));
    if (prePath) update(widgetRectForPath(*prePath));
    if (auto p = tool_->livePath()) update(widgetRectForPath(*p));
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
    // Restore the active tool's cursor rather than unsetCursor(), which
    // would leave a plain arrow until the next setToolCursor call.
    setCursor(toolCursor_);
    e->accept();
    return;
  }
  moveBrushCursorTo(QPointF(e->pos()));
  if (painting_ && tool_ && doc_ && e->button() == Qt::LeftButton) {
    std::optional<Rect> preRect = tool_->liveRect();
    std::optional<std::vector<Point2f>> prePath = tool_->livePath();

    const QPointF ip = (QPointF(e->pos()) - pan_) / zoom_;
    tool_->release(*doc_, static_cast<float>(ip.x()),
                   static_cast<float>(ip.y()), MouseButton::Left);
    painting_ = false;
    const Rect dirty = tool_->takeDirtyRect();
    if (!dirty.isEmpty()) requestRecomposite(dirty);
    // Wipe any lingering rubber-band / live polyline.
    if (preRect) update(widgetRectForPixels(*preRect));
    if (prePath) update(widgetRectForPath(*prePath));
    emit layerPainted();
    e->accept();
    return;
  }
  QWidget::mouseReleaseEvent(e);
}

}  // namespace tuxels
