#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QTemporaryDir>
#include <cmath>
#include <cstdio>

#include "io/PngIO.h"
#include "test_harness.h"

namespace tuxels {

namespace {

// Tolerance for 8-bit round-trip: 1/255 ≈ 0.004 so ±0.005 is safe.
constexpr float kRoundTripEps = 0.005f;

TuxImage makeCheckerboard(int w, int h) {
  TuxImage img(w, h);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const bool on = ((x / 4) + (y / 4)) & 1;
      img.setPixel(x, y,
                   on ? Rgba32F(1.f, 0.25f, 0.0f, 1.0f)
                      : Rgba32F(0.1f, 0.5f, 0.9f, 0.5f));
    }
  }
  return img;
}

}  // namespace

TEST(save_then_load_preserves_pixels) {
  QTemporaryDir tmp;
  CHECK(tmp.isValid());
  const QString path = tmp.filePath("roundtrip.png");

  TuxImage src = makeCheckerboard(32, 24);
  QString err;
  CHECK(savePng(path, src, &err));

  auto loaded = loadPng(path, &err);
  CHECK(loaded.has_value());
  CHECK_EQ(loaded->width(), 32);
  CHECK_EQ(loaded->height(), 24);

  for (int y = 0; y < src.height(); ++y) {
    for (int x = 0; x < src.width(); ++x) {
      Rgba32F a = src.getPixel(x, y);
      Rgba32F b = loaded->getPixel(x, y);
      CHECK_NEAR(a.r, b.r, kRoundTripEps);
      CHECK_NEAR(a.g, b.g, kRoundTripEps);
      CHECK_NEAR(a.b, b.b, kRoundTripEps);
      CHECK_NEAR(a.a, b.a, kRoundTripEps);
    }
  }
}

TEST(save_clamps_out_of_range_floats) {
  QTemporaryDir tmp;
  CHECK(tmp.isValid());
  const QString path = tmp.filePath("clamp.png");

  TuxImage src(2, 1);
  src.setPixel(0, 0, Rgba32F(-0.5f, 2.0f, 0.5f, 1.0f));
  src.setPixel(1, 0, Rgba32F(1.5f, -1.f, 0.25f, 0.75f));
  CHECK(savePng(path, src));

  auto loaded = loadPng(path);
  CHECK(loaded.has_value());

  Rgba32F p0 = loaded->getPixel(0, 0);
  CHECK_NEAR(p0.r, 0.f, kRoundTripEps);
  CHECK_NEAR(p0.g, 1.f, kRoundTripEps);
  CHECK_NEAR(p0.b, 0.5f, kRoundTripEps);
  CHECK_NEAR(p0.a, 1.f, kRoundTripEps);

  Rgba32F p1 = loaded->getPixel(1, 0);
  CHECK_NEAR(p1.r, 1.f, kRoundTripEps);
  CHECK_NEAR(p1.g, 0.f, kRoundTripEps);
}

TEST(load_nonexistent_reports_error) {
  QString err;
  auto loaded = loadPng("/tmp/does-not-exist-tuxels.png", &err);
  CHECK(!loaded.has_value());
  CHECK(!err.isEmpty());
}

TEST(save_empty_image_fails_cleanly) {
  QTemporaryDir tmp;
  CHECK(tmp.isValid());
  TuxImage empty(0, 0);
  QString err;
  CHECK(!savePng(tmp.filePath("empty.png"), empty, &err));
  CHECK(!err.isEmpty());
}

}  // namespace tuxels

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  return tuxels::testing::run();
}
