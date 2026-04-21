#include "geom/Spline.h"

#include <algorithm>
#include <cmath>

namespace tuxels {

namespace {

// Cubic Hermite basis functions (h00/h10/h01/h11 per Wikipedia). The `h`
// factor for the tangent terms is folded in by the caller.
inline float h00(float t) { return 2.f * t * t * t - 3.f * t * t + 1.f; }
inline float h10(float t) { return t * t * t - 2.f * t * t + t; }
inline float h01(float t) { return -2.f * t * t * t + 3.f * t * t; }
inline float h11(float t) { return t * t * t - t * t; }

// Expand the caller's point list with implied endpoints at (0,0) / (1,1).
// Returns a fresh vector so we don't mutate caller state. Duplicate x values
// get a tiny epsilon nudge so the segment lookup never divides by zero.
std::vector<SplinePoint> normalisePoints(const std::vector<SplinePoint>& in) {
  std::vector<SplinePoint> pts;
  pts.reserve(in.size() + 2);
  if (in.empty() || in.front().x > 0.f) pts.push_back({0.f, 0.f});
  for (const auto& p : in) pts.push_back(p);
  if (pts.empty() || pts.back().x < 1.f) pts.push_back({1.f, 1.f});
  // Guard against duplicate x values that would divide by zero below.
  for (std::size_t i = 1; i < pts.size(); ++i) {
    if (pts[i].x <= pts[i - 1].x) pts[i].x = pts[i - 1].x + 1e-6f;
  }
  return pts;
}

// Fritsch-Carlson monotone-preserving tangent computation. Returns one
// tangent per point in `pts`, in the same order.
std::vector<float> computeTangents(const std::vector<SplinePoint>& pts) {
  const std::size_t n = pts.size();
  std::vector<float> d(n - 1);  // secant slopes
  for (std::size_t i = 0; i + 1 < n; ++i) {
    const float dx = pts[i + 1].x - pts[i].x;
    d[i] = (pts[i + 1].y - pts[i].y) / dx;
  }
  std::vector<float> m(n);
  m[0] = d[0];
  m[n - 1] = d[n - 2];
  for (std::size_t i = 1; i + 1 < n; ++i) {
    if (d[i - 1] * d[i] <= 0.f) {
      // Sign change → pin this point's tangent to 0 to preserve monotonicity
      // (a local extremum that would otherwise overshoot).
      m[i] = 0.f;
    } else {
      m[i] = 0.5f * (d[i - 1] + d[i]);
    }
  }
  // Fritsch-Carlson limiter: scale tangents down when they're too steep for
  // the local segment, so the cubic stays monotone within each segment.
  for (std::size_t i = 0; i + 1 < n; ++i) {
    if (d[i] == 0.f) {
      m[i] = 0.f;
      m[i + 1] = 0.f;
      continue;
    }
    const float alpha = m[i] / d[i];
    const float beta = m[i + 1] / d[i];
    const float s2 = alpha * alpha + beta * beta;
    if (s2 > 9.f) {
      const float tau = 3.f / std::sqrt(s2);
      m[i] = tau * alpha * d[i];
      m[i + 1] = tau * beta * d[i];
    }
  }
  return m;
}

float evaluateImpl(const std::vector<SplinePoint>& pts,
                   const std::vector<float>& m, float x) {
  if (x <= pts.front().x) return pts.front().y;
  if (x >= pts.back().x) return pts.back().y;
  // Linear scan — curves carry a handful of points, binary search would be
  // overkill. buildLut256 stays O(n × 256) which is plenty fast.
  std::size_t i = 0;
  while (i + 1 < pts.size() && pts[i + 1].x < x) ++i;
  const float h = pts[i + 1].x - pts[i].x;
  const float t = (x - pts[i].x) / h;
  return h00(t) * pts[i].y + h10(t) * h * m[i] +
         h01(t) * pts[i + 1].y + h11(t) * h * m[i + 1];
}

}  // namespace

void buildLut256(const std::vector<SplinePoint>& in, uint8_t* out) {
  const auto pts = normalisePoints(in);
  const auto m = computeTangents(pts);
  for (int i = 0; i < 256; ++i) {
    const float x = static_cast<float>(i) / 255.f;
    float y = evaluateImpl(pts, m, x);
    y = std::clamp(y, 0.f, 1.f);
    out[i] = static_cast<uint8_t>(std::lround(y * 255.f));
  }
}

float evaluateSpline(const std::vector<SplinePoint>& in, float x) {
  const auto pts = normalisePoints(in);
  const auto m = computeTangents(pts);
  return evaluateImpl(pts, m, x);
}

}  // namespace tuxels
