#include <cmath>

#include "core/Histogram.h"
#include "core/Pixel.h"
#include "core/SelectionMask.h"
#include "core/TuxImage.h"
#include "test_harness.h"

using namespace tuxels;

namespace {
constexpr Rgba32F kRed{1.f, 0.f, 0.f, 1.f};
constexpr Rgba32F kBlack{0.f, 0.f, 0.f, 1.f};
constexpr Rgba32F kTransparent{0.f, 0.f, 0.f, 0.f};
}  // namespace

TEST(histogram_empty_zero_sized_image_is_zero) {
  TuxImage img(0, 0);
  Histogram4x256 h = computeHistogram(img);
  CHECK_EQ(h.total, 0ull);
  for (int c = 0; c < 4; ++c)
    for (int b = 0; b < 256; ++b) CHECK_EQ(h.buckets[c][b], 0u);
}

TEST(histogram_constant_red_bins_each_channel_once) {
  TuxImage img(32, 32);
  img.fill(kRed);
  Histogram4x256 h = computeHistogram(img);
  CHECK_EQ(h.total, 1024ull);
  CHECK_EQ(h.buckets[0][255], 1024u);
  CHECK_EQ(h.buckets[1][0], 1024u);
  CHECK_EQ(h.buckets[2][0], 1024u);
  // Luma of pure red: 0.299 → lround(0.299*255) = 76.
  CHECK_EQ(h.buckets[3][76], 1024u);
  // Nothing bled into adjacent buckets.
  CHECK_EQ(h.buckets[0][254], 0u);
  CHECK_EQ(h.buckets[3][75], 0u);
  CHECK_EQ(h.buckets[3][77], 0u);
}

TEST(histogram_fully_transparent_contributes_nothing) {
  TuxImage img(32, 32);
  img.fill(kTransparent);
  Histogram4x256 h = computeHistogram(img);
  CHECK_EQ(h.total, 0ull);
  for (int c = 0; c < 4; ++c)
    for (int b = 0; b < 256; ++b) CHECK_EQ(h.buckets[c][b], 0u);
}

TEST(histogram_black_opaque_hits_zero_bucket) {
  TuxImage img(16, 16);
  img.fill(kBlack);
  Histogram4x256 h = computeHistogram(img);
  CHECK_EQ(h.total, 256ull);
  CHECK_EQ(h.buckets[0][0], 256u);
  CHECK_EQ(h.buckets[1][0], 256u);
  CHECK_EQ(h.buckets[2][0], 256u);
  CHECK_EQ(h.buckets[3][0], 256u);
}

TEST(histogram_selection_clip_counts_only_selected_region) {
  TuxImage img(32, 32);
  img.fill(kRed);
  SelectionMask clip(32, 32);
  // Left half selected (x ∈ [0, 16)); right half deselected by default.
  clip.fillRect({0, 0, 16, 32}, 1.f);
  Histogram4x256 h = computeHistogram(img, &clip);
  CHECK_EQ(h.total, 512ull);
  CHECK_EQ(h.buckets[0][255], 512u);
  CHECK_EQ(h.buckets[1][0], 512u);
  CHECK_EQ(h.buckets[2][0], 512u);
  CHECK_EQ(h.buckets[3][76], 512u);
}

TEST(histogram_spans_multiple_tiles_without_double_counting) {
  // 300×300 doc spans a 2×2 tile grid where every edge tile is materialized
  // with out-of-doc pixels by TuxImage::fill. The per-tile clip bounds
  // ensure each of the 90000 doc pixels is counted exactly once.
  TuxImage img(300, 300);
  img.fill(kRed);
  Histogram4x256 h = computeHistogram(img);
  CHECK_EQ(h.total, 90000ull);
  CHECK_EQ(h.buckets[0][255], 90000u);
  CHECK_EQ(h.buckets[1][0], 90000u);
  CHECK_EQ(h.buckets[2][0], 90000u);
}

TEST(histogram_clamps_out_of_range_channels) {
  // Pathological floats (super-white, negative) should pin to 255/0 rather
  // than wrapping or going out of bucket bounds. Defends the 256-bucket
  // invariant if a future caller forgets to clamp an HDR float.
  TuxImage img(4, 4);
  img.fill({2.f, -0.5f, 0.5f, 1.f});
  Histogram4x256 h = computeHistogram(img);
  CHECK_EQ(h.total, 16ull);
  CHECK_EQ(h.buckets[0][255], 16u);
  CHECK_EQ(h.buckets[1][0], 16u);
  // 0.5 * 255 = 127.5 → lround rounds half-to-even in C++? Actually
  // `std::lround(0.5f*255.f) = std::lround(127.5f) = 128`. Accept either
  // 127 or 128 across IEEE-754 rounding modes.
  const uint32_t b127 = h.buckets[2][127];
  const uint32_t b128 = h.buckets[2][128];
  CHECK_EQ(b127 + b128, 16u);
}

int main() { return ::tuxels::testing::run(); }
