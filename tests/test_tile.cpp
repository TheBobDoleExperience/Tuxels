#include "core/Pixel.h"
#include "core/Tile.h"
#include "core/TileStore.h"
#include "core/TuxImage.h"
#include "test_harness.h"

using namespace tuxels;

TEST(pixel_defaults_to_transparent) {
  Rgba32F p;
  CHECK_EQ(p.r, 0.f);
  CHECK_EQ(p.a, 0.f);
  CHECK_EQ(p, Rgba32F::transparent());
}

TEST(pixel_constants) {
  CHECK_EQ(Rgba32F::black(), Rgba32F(0.f, 0.f, 0.f, 1.f));
  CHECK_EQ(Rgba32F::white(), Rgba32F(1.f, 1.f, 1.f, 1.f));
}

TEST(pixel_clamp) {
  Rgba32F hi(1.5f, -0.2f, 0.5f, 2.f);
  Rgba32F c = hi.clamped();
  CHECK_EQ(c, Rgba32F(1.f, 0.f, 0.5f, 1.f));
}

TEST(tile_fill_and_read) {
  Tile t;
  t.fill(Rgba32F(0.25f, 0.5f, 0.75f, 1.f));
  CHECK_EQ(t.at(0, 0), Rgba32F(0.25f, 0.5f, 0.75f, 1.f));
  CHECK_EQ(t.at(kTilePx - 1, kTilePx - 1), Rgba32F(0.25f, 0.5f, 0.75f, 1.f));
}

TEST(tile_clone_is_independent) {
  auto a = Tile::makeFilled(Rgba32F::black());
  auto b = a->clone();
  b->at(10, 10) = Rgba32F::white();
  CHECK_EQ(a->at(10, 10), Rgba32F::black());
  CHECK_EQ(b->at(10, 10), Rgba32F::white());
}

TEST(tile_coord_for_pixel_handles_negatives) {
  auto tc = [](int x, int y) { return TileCoord{x, y}; };
  CHECK_EQ(tileCoordForPixel(0, 0),                               tc(0, 0));
  CHECK_EQ(tileCoordForPixel(kTilePx - 1, kTilePx - 1),           tc(0, 0));
  CHECK_EQ(tileCoordForPixel(kTilePx, 0),                         tc(1, 0));
  CHECK_EQ(tileCoordForPixel(-1, -1),                             tc(-1, -1));
  CHECK_EQ(tileCoordForPixel(-kTilePx, -kTilePx),                 tc(-1, -1));
  CHECK_EQ(tileCoordForPixel(-kTilePx - 1, -kTilePx - 1),         tc(-2, -2));
}

TEST(local_in_tile_wraps_correctly) {
  CHECK_EQ(localInTile(0), 0);
  CHECK_EQ(localInTile(kTilePx - 1), kTilePx - 1);
  CHECK_EQ(localInTile(kTilePx), 0);
  CHECK_EQ(localInTile(-1), kTilePx - 1);
}

TEST(tilestore_starts_empty_and_sparse) {
  TileStore s;
  CHECK(s.empty());
  CHECK_EQ(s.find({0, 0}), nullptr);
}

TEST(tilestore_get_or_create_allocates_once) {
  TileStore s;
  Tile* a = s.getOrCreate({3, 4});
  Tile* b = s.getOrCreate({3, 4});
  CHECK_EQ(a, b);
  CHECK_EQ(s.size(), size_t(1));
}

TEST(tilestore_cow_swap_preserves_original) {
  TileStore s;
  Tile* original = s.getOrCreate({0, 0});
  original->fill(Rgba32F::black());
  auto snap = s.sharedAt({0, 0});
  auto fresh = snap->clone();
  fresh->fill(Rgba32F::white());
  s.set({0, 0}, fresh);
  CHECK_EQ(snap->at(0, 0), Rgba32F::black());
  CHECK_EQ(s.find({0, 0})->at(0, 0), Rgba32F::white());
}

TEST(tuximage_pixel_io_roundtrip) {
  TuxImage img(512, 384);
  CHECK_EQ(img.width(), 512);
  CHECK_EQ(img.height(), 384);
  CHECK_EQ(img.getPixel(100, 100), Rgba32F::transparent());
  img.setPixel(100, 100, Rgba32F(0.1f, 0.2f, 0.3f, 1.f));
  CHECK_EQ(img.getPixel(100, 100), Rgba32F(0.1f, 0.2f, 0.3f, 1.f));
}

TEST(tuximage_only_touched_tiles_allocated) {
  TuxImage img(2048, 2048);
  CHECK_EQ(img.tiles().size(), size_t(0));
  img.setPixel(0, 0, Rgba32F::white());
  img.setPixel(2047, 2047, Rgba32F::white());
  CHECK_EQ(img.tiles().size(), size_t(2));
}

TEST(tuximage_out_of_bounds_is_transparent_noop) {
  TuxImage img(64, 64);
  img.setPixel(-10, 5, Rgba32F::white());
  img.setPixel(999, 5, Rgba32F::white());
  CHECK(img.tiles().empty());
  CHECK_EQ(img.getPixel(-10, 5), Rgba32F::transparent());
  CHECK_EQ(img.getPixel(999, 5), Rgba32F::transparent());
}

TEST(tuximage_fill_spans_tiles) {
  TuxImage img(300, 300);  // spans 2x2 tiles with kTilePx=256
  img.fill(Rgba32F(0.3f, 0.3f, 0.3f, 1.f));
  CHECK_EQ(img.tiles().size(), size_t(4));
  CHECK_EQ(img.getPixel(0, 0), Rgba32F(0.3f, 0.3f, 0.3f, 1.f));
  CHECK_EQ(img.getPixel(299, 299), Rgba32F(0.3f, 0.3f, 0.3f, 1.f));
}

TEST(tuximage_tile_bounds) {
  TuxImage img(513, 257);  // spans 3x2 tiles
  Rect tb = img.tileBounds();
  CHECK_EQ(tb.w, 3);
  CHECK_EQ(tb.h, 2);
}

int main() { return tuxels::testing::run(); }
