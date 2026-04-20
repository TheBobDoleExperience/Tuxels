#pragma once

#include <QCursor>
#include <QImage>
#include <QLineF>
#include <QPoint>
#include <QPointF>
#include <QWidget>
#include <vector>

#include "core/TuxImage.h"
#include "tools/ToolId.h"

class QTimer;

namespace tuxels {

class Document;
class SelectionMask;
class ToolBase;
class TransformTool;

class CanvasView : public QWidget {
  Q_OBJECT

 public:
  explicit CanvasView(QWidget* parent = nullptr);

  void setDocument(Document* doc);
  void setTool(ToolBase* tool) noexcept { tool_ = tool; }
  // Wire the TransformTool so paintEvent can substitute its scratch image
  // into compose() via LayerOverride and draw the bbox + corner handles.
  void setTransformTool(TransformTool* t) noexcept { transformTool_ = t; }
  // Set the cursor shown while hovering the canvas with the active tool.
  // Stored so we can re-apply after a transient pan grab ends.
  void setToolCursor(const QCursor& c);
  // Build a Photoshop-style cursor for the given tool id (bucket, wand,
  // crop get custom pixmaps; marquee uses a crosshair; brush falls back to
  // the arrow since the ring overlay already shows the hot spot).
  static QCursor cursorForTool(ToolId id);
  void requestRecomposite();
  // Partial variant: union `pixelRect` into the pending dirty region so the
  // next paint recomposites only affected tiles. Empty rect is a no-op.
  void requestRecomposite(Rect pixelRect);
  // Repaint just the cursor-ring region. Call when the active tool's radius
  // changes (e.g. user adjusted brush size while hovering).
  void refreshBrushCursor();
  // Recompute the cached marching-ants edge segments from the document's
  // current selection. Call whenever the selection pointer changes (menu
  // ops, marquee commits, undo/redo).
  void refreshSelectionOverlay();

  double zoom() const noexcept { return zoom_; }
  void setZoom(double z);
  QPointF pan() const noexcept { return pan_; }

 signals:
  void zoomChanged(double z);
  void layerPainted();

 protected:
  void paintEvent(QPaintEvent*) override;
  void wheelEvent(QWheelEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mouseReleaseEvent(QMouseEvent*) override;
  void enterEvent(QEnterEvent*) override;
  void leaveEvent(QEvent*) override;
  QSize sizeHint() const override { return QSize(1024, 768); }

 private:
  void recomposite();
  void recompositePartial(Rect pixelRect);
  void ensureCacheImage(Rect pixelRect);
  QPointF canvasToWidget(QPointF p) const;
  QRect widgetRectForPixels(Rect pixelRect) const;
  // Widget-space bounding rect for the cursor ring centered at widget pos
  // `p`, sized to the active tool's radius. Empty if no radius is reported.
  QRect brushCursorWidgetRect(QPointF p) const;
  // Update `cursorWidgetPos_`, invalidating both the old and new ring
  // regions so the ring visibly follows the mouse.
  void moveBrushCursorTo(QPointF p);

  // Walk the doc's current SelectionMask and extract 4-connected boundary
  // segments into `selectionSegments_` (stored in doc pixel coords).
  void rebuildSelectionSegments();
  // Widget-space rect enclosing the selection outline (including ant pen
  // width). Empty when no selection. Used for timer-driven partial updates.
  QRect selectionWidgetRect() const;
  // Translate a Qt modifier bitmask to our tool-facing one.
  static int translateModifiers(int qtModifiers);

  Document* doc_ = nullptr;
  ToolBase* tool_ = nullptr;
  TransformTool* transformTool_ = nullptr;
  TuxImage composite_;
  QImage cache_;
  bool dirty_ = true;
  // Accumulated dirty rect for the next paint. Empty means "whole image"
  // when dirty_ is true; ignored when dirty_ is false.
  Rect dirtyRect_{};

  double zoom_ = 1.0;
  QPointF pan_ = {0.0, 0.0};
  bool panning_ = false;
  bool painting_ = false;
  QPoint panStart_;
  QPointF panStartOffset_;

  QPointF cursorWidgetPos_{-1.0, -1.0};
  bool cursorInCanvas_ = false;
  QCursor toolCursor_{Qt::ArrowCursor};

  // Marching-ants overlay.
  std::vector<QLineF> selectionSegments_;  // doc pixel coords
  Rect selectionBounds_{};                  // doc pixel bbox of selection
  const SelectionMask* lastSelection_ = nullptr;  // identity for cache
  QTimer* antsTimer_ = nullptr;
  int antsPhase_ = 0;
};

}  // namespace tuxels
