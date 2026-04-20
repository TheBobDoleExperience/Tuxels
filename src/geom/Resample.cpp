#include "geom/Resample.h"

#include <cmath>

namespace tuxels {

namespace {

inline Rgba32F premultiplied(const Rgba32F& p) {
  return Rgba32F{p.r * p.a, p.g * p.a, p.b * p.a, p.a};
}

// Threshold below which a blended alpha is treated as effectively
// transparent. Chosen above float-precision rotation noise (~1e-7) and
// well below 1/255 (~4e-3) so any alpha visible at 8-bit export survives.
constexpr float kAlphaEps = 1e-5f;

inline Rgba32F unpremultiplied(const Rgba32F& p) {
  if (p.a <= kAlphaEps) return Rgba32F::transparent();
  const float inv = 1.f / p.a;
  return Rgba32F{p.r * inv, p.g * inv, p.b * inv, p.a};
}

}  // namespace

void resampleBilinear(const TuxImage& src, TuxImage& dst,
                      const Affine2D& dstToSrc) {
  const int W = dst.width();
  const int H = dst.height();
  const int sW = src.width();
  const int sH = src.height();
  if (W <= 0 || H <= 0 || sW <= 0 || sH <= 0) return;

  TileRowCursor cur(src);

  for (int dy = 0; dy < H; ++dy) {
    for (int dx = 0; dx < W; ++dx) {
      float sx, sy;
      dstToSrc.mapPoint(static_cast<float>(dx) + 0.5f,
                        static_cast<float>(dy) + 0.5f, sx, sy);
      // Shift to a pixel-center-at-integer frame so std::floor picks the
      // top-left of the 2x2 kernel.
      const float fx = sx - 0.5f;
      const float fy = sy - 0.5f;
      const int i0 = static_cast<int>(std::floor(fx));
      const int j0 = static_cast<int>(std::floor(fy));
      // Skip dst pixels whose entire 2x2 kernel is outside the source.
      if (i0 + 1 < 0 || j0 + 1 < 0 || i0 >= sW || j0 >= sH) continue;
      const float tx = fx - static_cast<float>(i0);
      const float ty = fy - static_cast<float>(j0);

      const Rgba32F q00 = premultiplied(cur.at(i0, j0));
      const Rgba32F q10 = premultiplied(cur.at(i0 + 1, j0));
      const Rgba32F q01 = premultiplied(cur.at(i0, j0 + 1));
      const Rgba32F q11 = premultiplied(cur.at(i0 + 1, j0 + 1));

      const float w00 = (1.f - tx) * (1.f - ty);
      const float w10 = tx * (1.f - ty);
      const float w01 = (1.f - tx) * ty;
      const float w11 = tx * ty;

      Rgba32F blend;
      blend.r = q00.r * w00 + q10.r * w10 + q01.r * w01 + q11.r * w11;
      blend.g = q00.g * w00 + q10.g * w10 + q01.g * w01 + q11.g * w11;
      blend.b = q00.b * w00 + q10.b * w10 + q01.b * w01 + q11.b * w11;
      blend.a = q00.a * w00 + q10.a * w10 + q01.a * w01 + q11.a * w11;

      const Rgba32F out = unpremultiplied(blend);
      if (out.a > kAlphaEps) dst.setPixel(dx, dy, out);
    }
  }
}

}  // namespace tuxels
