#pragma once

#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QWidget>

#include "core/TuxImage.h"

namespace tuxels {

class Document;
class ToolBase;

class CanvasView : public QWidget {
  Q_OBJECT

 public:
  explicit CanvasView(QWidget* parent = nullptr);

  void setDocument(Document* doc);
  void setTool(ToolBase* tool) noexcept { tool_ = tool; }
  void requestRecomposite();

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
  QSize sizeHint() const override { return QSize(1024, 768); }

 private:
  void recomposite();
  void ensureCacheImage();
  QPointF canvasToWidget(QPointF p) const;

  Document* doc_ = nullptr;
  ToolBase* tool_ = nullptr;
  TuxImage composite_;
  QImage cache_;
  bool dirty_ = true;

  double zoom_ = 1.0;
  QPointF pan_ = {0.0, 0.0};
  bool panning_ = false;
  bool painting_ = false;
  QPoint panStart_;
  QPointF panStartOffset_;
};

}  // namespace tuxels
