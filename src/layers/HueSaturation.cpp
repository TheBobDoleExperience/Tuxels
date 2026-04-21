#include "layers/HueSaturation.h"

#include <algorithm>
#include <cmath>

#include "core/Tile.h"

namespace tuxels {

namespace {

// Standard HSL conversion matching PS's Master slider behavior. Hue in
// degrees [0, 360); saturation and lightness in [0, 1].
void rgbToHsl(float r, float g, float b, float* h, float* s, float* l) {
  const float mx = std::max({r, g, b});
  const float mn = std::min({r, g, b});
  const float sum = mx + mn;
  const float diff = mx - mn;

  *l = sum * 0.5f;

  if (diff <= 0.f) {
    *h = 0.f;
    *s = 0.f;
    return;
  }

  *s = (*l > 0.5f) ? (diff / (2.f - sum)) : (diff / sum);

  float hue;
  if (mx == r) {
    hue = (g - b) / diff + (g < b ? 6.f : 0.f);
  } else if (mx == g) {
    hue = (b - r) / diff + 2.f;
  } else {
    hue = (r - g) / diff + 4.f;
  }
  *h = hue * 60.f;
}

float hueToRgb(float p, float q, float t) {
  if (t < 0.f) t += 1.f;
  if (t > 1.f) t -= 1.f;
  if (t < 1.f / 6.f) return p + (q - p) * 6.f * t;
  if (t < 0.5f) return q;
  if (t < 2.f / 3.f) return p + (q - p) * (2.f / 3.f - t) * 6.f;
  return p;
}

void hslToRgb(float h, float s, float l, float* r, float* g, float* b) {
  if (s <= 0.f) {
    *r = *g = *b = l;
    return;
  }
  const float q = (l < 0.5f) ? (l * (1.f + s)) : (l + s - l * s);
  const float p = 2.f * l - q;
  const float hn = h / 360.f;
  *r = hueToRgb(p, q, hn + 1.f / 3.f);
  *g = hueToRgb(p, q, hn);
  *b = hueToRgb(p, q, hn - 1.f / 3.f);
}

}  // namespace

HueSaturation::HueSaturation() = default;

void HueSaturation::setParams(const HueSaturationParams& p) { params_ = p; }

void HueSaturation::applyToAccum(TileCoord /*tc*/, Rgba32F* accum) const {
  // Fully-transparent pixels are skipped — their RGB is undefined / premul-
  // zeroed and running HSL on them is wasted work that can also produce
  // spurious color during later blending.
  for (int i = 0; i < kTilePixels; ++i) {
    Rgba32F& p = accum[i];
    if (p.a <= 0.f) continue;

    const float r0 = std::clamp(p.r, 0.f, 1.f);
    const float g0 = std::clamp(p.g, 0.f, 1.f);
    const float b0 = std::clamp(p.b, 0.f, 1.f);

    float h;
    float s;
    float l;
    rgbToHsl(r0, g0, b0, &h, &s, &l);

    h = std::fmod(h + params_.hueShift + 360.f, 360.f);
    s = std::clamp(s * (1.f + params_.saturation), 0.f, 1.f);
    l = std::clamp(l + params_.lightness, 0.f, 1.f);

    float r;
    float g;
    float b;
    hslToRgb(h, s, l, &r, &g, &b);
    p.r = r;
    p.g = g;
    p.b = b;
  }
}

}  // namespace tuxels
