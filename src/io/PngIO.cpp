#include "io/PngIO.h"

#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <algorithm>
#include <cmath>

namespace tuxels {

namespace {

inline quint8 quantize(float v) {
  const float clamped = std::clamp(v, 0.f, 1.f);
  return static_cast<quint8>(std::lround(clamped * 255.f));
}

}  // namespace

std::optional<TuxImage> loadPng(const QString& path, QString* err) {
  QImageReader reader(path);
  reader.setAutoTransform(true);
  QImage src = reader.read();
  if (src.isNull()) {
    if (err) *err = reader.errorString();
    return std::nullopt;
  }
  src = src.convertToFormat(QImage::Format_RGBA8888);
  if (src.isNull()) {
    if (err) *err = QStringLiteral("format conversion failed");
    return std::nullopt;
  }

  const int w = src.width();
  const int h = src.height();
  TuxImage out(w, h);
  for (int y = 0; y < h; ++y) {
    const uchar* row = src.constScanLine(y);
    for (int x = 0; x < w; ++x) {
      const uchar* p = row + 4 * x;
      out.setPixel(x, y,
                   Rgba32F(p[0] / 255.f, p[1] / 255.f, p[2] / 255.f, p[3] / 255.f));
    }
  }
  return out;
}

bool savePng(const QString& path, const TuxImage& image, QString* err) {
  const int w = image.width();
  const int h = image.height();
  if (w <= 0 || h <= 0) {
    if (err) *err = QStringLiteral("image has zero extent");
    return false;
  }

  QImage dst(w, h, QImage::Format_RGBA8888);
  for (int y = 0; y < h; ++y) {
    uchar* row = dst.scanLine(y);
    for (int x = 0; x < w; ++x) {
      Rgba32F c = image.getPixel(x, y);
      uchar* p = row + 4 * x;
      p[0] = quantize(c.r);
      p[1] = quantize(c.g);
      p[2] = quantize(c.b);
      p[3] = quantize(c.a);
    }
  }

  QImageWriter writer(path, "png");
  if (!writer.write(dst)) {
    if (err) *err = writer.errorString();
    return false;
  }
  return true;
}

}  // namespace tuxels
