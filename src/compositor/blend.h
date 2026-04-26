#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "compositor/BlendMode.h"
#include "core/Pixel.h"

namespace tuxels {

// Per-channel blend functions. Inputs in [0, 1]. Output unclamped (caller may
// clamp at the boundary). Formulas match Adobe PDF / Photoshop reference.

inline float bm_normal    (float /*cb*/, float cs) { return cs; }
inline float bm_multiply  (float cb, float cs) { return cb * cs; }
inline float bm_screen    (float cb, float cs) { return 1.f - (1.f - cb) * (1.f - cs); }
inline float bm_darken    (float cb, float cs) { return std::min(cb, cs); }
inline float bm_lighten   (float cb, float cs) { return std::max(cb, cs); }
inline float bm_difference(float cb, float cs) { return std::fabs(cb - cs); }
inline float bm_exclusion (float cb, float cs) { return cb + cs - 2.f * cb * cs; }

inline float bm_color_burn(float cb, float cs) {
  if (cb >= 1.f) return 1.f;
  if (cs <= 0.f) return 0.f;
  return 1.f - std::min(1.f, (1.f - cb) / cs);
}
inline float bm_color_dodge(float cb, float cs) {
  if (cb <= 0.f) return 0.f;
  if (cs >= 1.f) return 1.f;
  return std::min(1.f, cb / (1.f - cs));
}
inline float bm_overlay(float cb, float cs) {
  return cb <= 0.5f ? 2.f * cb * cs
                    : 1.f - 2.f * (1.f - cb) * (1.f - cs);
}
inline float bm_hard_light(float cb, float cs) {
  return cs <= 0.5f ? 2.f * cb * cs
                    : 1.f - 2.f * (1.f - cb) * (1.f - cs);
}
inline float bm_soft_light(float cb, float cs) {
  // Photoshop / PDF-spec soft light.
  const float d = (cb <= 0.25f)
                      ? ((16.f * cb - 12.f) * cb + 4.f) * cb
                      : std::sqrt(cb);
  return (cs <= 0.5f)
             ? cb - (1.f - 2.f * cs) * cb * (1.f - cb)
             : cb + (2.f * cs - 1.f) * (d - cb);
}

// Dispatch table — per-channel blend value for a given mode. Dissolve is
// handled specially by the compositor (it manipulates alpha, not color) and
// this function treats it as Normal. Pass-Through is handled by the compose
// recursion and never reaches per-pixel blend; falls back to Normal as a
// safety net.
inline float applyBlend(BlendMode mode, float cb, float cs) {
  switch (mode) {
    case BlendMode::Normal:      return bm_normal(cb, cs);
    case BlendMode::Dissolve:    return bm_normal(cb, cs);
    case BlendMode::Darken:      return bm_darken(cb, cs);
    case BlendMode::Multiply:    return bm_multiply(cb, cs);
    case BlendMode::ColorBurn:   return bm_color_burn(cb, cs);
    case BlendMode::Lighten:     return bm_lighten(cb, cs);
    case BlendMode::Screen:      return bm_screen(cb, cs);
    case BlendMode::ColorDodge:  return bm_color_dodge(cb, cs);
    case BlendMode::Overlay:     return bm_overlay(cb, cs);
    case BlendMode::SoftLight:   return bm_soft_light(cb, cs);
    case BlendMode::HardLight:   return bm_hard_light(cb, cs);
    case BlendMode::Difference:  return bm_difference(cb, cs);
    case BlendMode::Exclusion:   return bm_exclusion(cb, cs);
    case BlendMode::PassThrough: return bm_normal(cb, cs);
  }
  return cs;
}

// Small deterministic hash used by Dissolve so the pattern is stable across
// redraws for a given (seed, x, y).
inline uint32_t dissolveHash(uint32_t seed, int x, int y) {
  uint32_t h = seed + static_cast<uint32_t>(x) * 0x9E3779B1u +
               static_cast<uint32_t>(y) * 0x85EBCA77u;
  h ^= h >> 15;
  h *= 0xC2B2AE35u;
  h ^= h >> 13;
  return h;
}

// Full per-pixel composite: backdrop + source → output, given the layer's
// blend mode, effective opacity (layer.opacity × mask), and a noise seed +
// absolute pixel coordinates for Dissolve.
inline Rgba32F compositePixel(Rgba32F backdrop, Rgba32F source, BlendMode mode,
                              float layerAlphaFactor, uint32_t seed, int absX,
                              int absY) {
  const float as = source.a * layerAlphaFactor;
  if (as <= 0.f) return backdrop;

  if (mode == BlendMode::Dissolve) {
    const uint32_t h = dissolveHash(seed, absX, absY);
    const float r = static_cast<float>(h >> 8) * (1.f / 16777216.f);
    if (r < as) {
      return {source.r, source.g, source.b, std::max(backdrop.a, 1.f)};
    }
    return backdrop;
  }

  const float ab = backdrop.a;
  const float br = applyBlend(mode, backdrop.r, source.r);
  const float bg = applyBlend(mode, backdrop.g, source.g);
  const float bb = applyBlend(mode, backdrop.b, source.b);

  // "Effective source color" — blended with backdrop weighted by backdrop alpha.
  const float sr = (1.f - ab) * source.r + ab * br;
  const float sg = (1.f - ab) * source.g + ab * bg;
  const float sb = (1.f - ab) * source.b + ab * bb;

  const float ar = as + ab * (1.f - as);
  if (ar <= 0.f) return Rgba32F::transparent();
  const float inv = 1.f / ar;
  return {
      (as * sr + ab * backdrop.r * (1.f - as)) * inv,
      (as * sg + ab * backdrop.g * (1.f - as)) * inv,
      (as * sb + ab * backdrop.b * (1.f - as)) * inv,
      ar,
  };
}

}  // namespace tuxels
