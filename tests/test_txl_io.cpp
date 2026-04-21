#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "compositor/BlendMode.h"
#include "core/Document.h"
#include "core/Pixel.h"
#include "core/SelectionMask.h"
#include "core/TuxImage.h"
#include "geom/Spline.h"
#include "io/TxlIO.h"
#include "layers/CurvesAdjustment.h"
#include "layers/LayerMask.h"
#include "layers/LevelsAdjustment.h"
#include "layers/PixelLayer.h"
#include "test_harness.h"

namespace tuxels {

namespace {

constexpr Rgba32F kRed{1.f, 0.f, 0.f, 1.f};
constexpr Rgba32F kGreen{0.f, 1.f, 0.f, 1.f};
constexpr Rgba32F kBlue{0.f, 0.f, 1.f, 1.f};

std::string tmpPath(const char* stem) {
  static int counter = 0;
  auto dir = std::filesystem::temp_directory_path();
  auto p =
      dir / (std::string("tuxels_txl_") + stem + "_" +
             std::to_string(++counter) + ".txl");
  return p.string();
}

struct PathGuard {
  std::string path;
  ~PathGuard() { std::remove(path.c_str()); }
};

}  // namespace

TEST(txl_round_trip_preserves_document_dimensions_and_active_layer) {
  const std::string path = tmpPath("dims");
  PathGuard g{path};

  Document src(123, 45);
  src.addBlankPixelLayer("A");
  src.addBlankPixelLayer("B");
  src.setActiveLayerIndex(0);

  std::string err;
  CHECK(saveTxl(path, src, &err));

  auto loaded = loadTxl(path, &err);
  CHECK(loaded.has_value());
  CHECK_EQ((*loaded)->width(), 123);
  CHECK_EQ((*loaded)->height(), 45);
  CHECK_EQ((*loaded)->activeLayerIndex(), 0);
  CHECK_EQ(static_cast<int>((*loaded)->tree().size()), 2);
}

TEST(txl_round_trip_preserves_layer_metadata) {
  const std::string path = tmpPath("meta");
  PathGuard g{path};

  Document src(40, 40);
  auto* a = src.addBlankPixelLayer("First");
  a->visible = false;
  a->opacity = 0.42f;
  a->blend = BlendMode::Multiply;
  auto* b = src.addBlankPixelLayer("Second");
  b->opacity = 0.75f;
  b->blend = BlendMode::Screen;

  std::string err;
  CHECK(saveTxl(path, src, &err));
  auto loaded = loadTxl(path, &err);
  CHECK(loaded.has_value());

  const auto& tree = (*loaded)->tree();
  CHECK_EQ(static_cast<int>(tree.size()), 2);
  CHECK(tree.at(0)->name == "First");
  CHECK(tree.at(0)->visible == false);
  CHECK_NEAR(tree.at(0)->opacity, 0.42f, 1e-5f);
  CHECK(tree.at(0)->blend == BlendMode::Multiply);
  CHECK(tree.at(1)->name == "Second");
  CHECK(tree.at(1)->visible == true);
  CHECK_NEAR(tree.at(1)->opacity, 0.75f, 1e-5f);
  CHECK(tree.at(1)->blend == BlendMode::Screen);
}

TEST(txl_round_trip_preserves_pixel_content) {
  const std::string path = tmpPath("pixels");
  PathGuard g{path};

  Document src(300, 300);  // spans multiple 256×256 tiles
  auto* layer = src.addBlankPixelLayer("L");
  // Unique per-pixel pattern so mismatches are detectable.
  for (int y = 0; y < 300; ++y) {
    for (int x = 0; x < 300; ++x) {
      const float r = static_cast<float>((x * 7 + y * 3) % 251) / 250.f;
      const float gc = static_cast<float>((x * 11 + y * 5) % 241) / 240.f;
      layer->image.setPixel(x, y, Rgba32F{r, gc, 0.25f, 1.f});
    }
  }

  std::string err;
  CHECK(saveTxl(path, src, &err));
  auto loaded = loadTxl(path, &err);
  CHECK(loaded.has_value());

  auto* loadedLayer =
      dynamic_cast<PixelLayer*>((*loaded)->tree().at(0));
  CHECK(loadedLayer != nullptr);
  // Spot-check across tile boundaries.
  for (auto [x, y] : std::initializer_list<std::pair<int, int>>{
           {0, 0}, {255, 255}, {256, 256}, {299, 299}, {100, 200}, {200, 100}}) {
    const float expR = static_cast<float>((x * 7 + y * 3) % 251) / 250.f;
    const float expG = static_cast<float>((x * 11 + y * 5) % 241) / 240.f;
    Rgba32F p = loadedLayer->image.getPixel(x, y);
    CHECK_NEAR(p.r, expR, 1e-6f);
    CHECK_NEAR(p.g, expG, 1e-6f);
    CHECK_NEAR(p.b, 0.25f, 1e-6f);
    CHECK_NEAR(p.a, 1.f, 1e-6f);
  }
}

TEST(txl_round_trip_preserves_tile_sparsity) {
  const std::string path = tmpPath("sparse");
  PathGuard g{path};

  // A 1024×1024 doc has 4×4 tiles. Touch only a couple — expect the
  // loaded image to keep the absent ones absent.
  Document src(1024, 1024);
  auto* layer = src.addBlankPixelLayer("L");
  layer->image.setPixel(10, 10, kRed);
  layer->image.setPixel(800, 800, kGreen);
  const std::size_t writtenTiles = layer->image.tiles().size();
  CHECK_EQ(static_cast<int>(writtenTiles), 2);

  std::string err;
  CHECK(saveTxl(path, src, &err));
  auto loaded = loadTxl(path, &err);
  CHECK(loaded.has_value());

  auto* loadedLayer =
      dynamic_cast<PixelLayer*>((*loaded)->tree().at(0));
  CHECK(loadedLayer != nullptr);
  CHECK_EQ(static_cast<int>(loadedLayer->image.tiles().size()), 2);
  CHECK_NEAR(loadedLayer->image.getPixel(10, 10).r, 1.f, 1e-6f);
  CHECK_NEAR(loadedLayer->image.getPixel(800, 800).g, 1.f, 1e-6f);
  // An untouched area reads as transparent (absent tile).
  CHECK_NEAR(loadedLayer->image.getPixel(500, 500).a, 0.f, 1e-6f);
}

TEST(txl_round_trip_preserves_layer_mask) {
  const std::string path = tmpPath("mask");
  PathGuard g{path};

  Document src(50, 50);
  auto* layer = src.addBlankPixelLayer("L");
  layer->image.fill(kRed);
  auto mask = std::make_unique<LayerMask>(50, 50);
  mask->enabled = false;
  for (int y = 0; y < 50; ++y)
    for (int x = 25; x < 50; ++x)
      mask->image.setPixel(x, y, Rgba32F(0.f, 0.f, 0.f, 1.f));
  layer->mask = std::move(mask);

  std::string err;
  CHECK(saveTxl(path, src, &err));
  auto loaded = loadTxl(path, &err);
  CHECK(loaded.has_value());

  auto* loadedLayer =
      dynamic_cast<PixelLayer*>((*loaded)->tree().at(0));
  CHECK(loadedLayer != nullptr);
  CHECK(loadedLayer->mask != nullptr);
  CHECK(loadedLayer->mask->enabled == false);
  CHECK_NEAR(loadedLayer->mask->image.getPixel(30, 30).r, 0.f, 1e-6f);
  // Left half is tile-absent (=0 on getPixel, which is fine for this check).
}

TEST(txl_round_trip_preserves_selection) {
  const std::string path = tmpPath("sel");
  PathGuard g{path};

  Document src(60, 60);
  src.addBlankPixelLayer("L");
  auto sel = std::make_unique<SelectionMask>(60, 60);
  sel->fillRect(Rect{10, 10, 20, 20}, 1.f);
  src.setSelection(std::move(sel));

  std::string err;
  CHECK(saveTxl(path, src, &err));
  auto loaded = loadTxl(path, &err);
  CHECK(loaded.has_value());

  CHECK((*loaded)->selection() != nullptr);
  CHECK_NEAR((*loaded)->selection()->sample(15, 15), 1.f, 1e-6f);
  CHECK_NEAR((*loaded)->selection()->sample(5, 5), 0.f, 1e-6f);
}

TEST(txl_round_trip_preserves_paint_target) {
  const std::string path = tmpPath("target");
  PathGuard g{path};

  Document src(40, 40);
  auto* layer = src.addBlankPixelLayer("L");
  layer->mask = std::make_unique<LayerMask>(40, 40);
  src.setPaintTarget(PaintTarget::Mask);

  std::string err;
  CHECK(saveTxl(path, src, &err));
  auto loaded = loadTxl(path, &err);
  CHECK(loaded.has_value());
  CHECK((*loaded)->paintTarget() == PaintTarget::Mask);
}

TEST(txl_round_trip_doc_without_selection_reloads_with_null) {
  const std::string path = tmpPath("nosel");
  PathGuard g{path};

  Document src(20, 20);
  src.addBlankPixelLayer("L");

  std::string err;
  CHECK(saveTxl(path, src, &err));
  auto loaded = loadTxl(path, &err);
  CHECK(loaded.has_value());
  CHECK((*loaded)->selection() == nullptr);
}

TEST(txl_round_trip_empty_document_survives) {
  const std::string path = tmpPath("empty");
  PathGuard g{path};

  Document src(10, 10);  // no layers

  std::string err;
  CHECK(saveTxl(path, src, &err));
  auto loaded = loadTxl(path, &err);
  CHECK(loaded.has_value());
  CHECK_EQ((*loaded)->width(), 10);
  CHECK_EQ(static_cast<int>((*loaded)->tree().size()), 0);
  CHECK_EQ((*loaded)->activeLayerIndex(), -1);
}

TEST(txl_round_trip_preserves_layer_id_counter) {
  const std::string path = tmpPath("ids");
  PathGuard g{path};

  Document src(20, 20);
  src.addBlankPixelLayer("One");   // id = 1
  src.addBlankPixelLayer("Two");   // id = 2
  src.addBlankPixelLayer("Three"); // id = 3

  std::string err;
  CHECK(saveTxl(path, src, &err));
  auto loaded = loadTxl(path, &err);
  CHECK(loaded.has_value());
  // nextLayerId() must not collide with any loaded id.
  const LayerId fresh = (*loaded)->nextLayerId();
  CHECK(fresh > 3);
}

TEST(txl_round_trip_preserves_layer_origin_and_dims) {
  // v2 format: per-layer width/height + doc-coord origin must survive a
  // save-load round-trip. Important for M2-S1 Place Image, where layers no
  // longer match doc dims.
  const std::string path = tmpPath("origin");
  PathGuard g{path};

  Document src(200, 200);
  // Blank layer (doc-sized, origin 0) to exercise the default path.
  src.addBlankPixelLayer("BG");
  // Placed layer: 80×60 at origin (120, 30).
  TuxImage placed(80, 60);
  placed.fill(kGreen);
  src.addPixelLayer(std::move(placed), 120, 30, "Placed");

  std::string err;
  CHECK(saveTxl(path, src, &err));
  auto loaded = loadTxl(path, &err);
  CHECK(loaded.has_value());
  CHECK_EQ(static_cast<int>((*loaded)->tree().size()), 2);

  auto* bg = dynamic_cast<PixelLayer*>((*loaded)->tree().at(0));
  CHECK(bg != nullptr);
  CHECK_EQ(bg->originX, 0);
  CHECK_EQ(bg->originY, 0);
  CHECK_EQ(bg->image.width(), 200);
  CHECK_EQ(bg->image.height(), 200);

  auto* fg = dynamic_cast<PixelLayer*>((*loaded)->tree().at(1));
  CHECK(fg != nullptr);
  CHECK_EQ(fg->originX, 120);
  CHECK_EQ(fg->originY, 30);
  CHECK_EQ(fg->image.width(), 80);
  CHECK_EQ(fg->image.height(), 60);
  CHECK_NEAR(fg->image.getPixel(0, 0).g, 1.f, 1e-6f);
  CHECK_NEAR(fg->image.getPixel(79, 59).g, 1.f, 1e-6f);
}

TEST(txl_load_rejects_missing_file) {
  std::string err;
  auto loaded = loadTxl("/tmp/does-not-exist-tuxels-txl.txl", &err);
  CHECK(!loaded.has_value());
  CHECK(!err.empty());
}

TEST(txl_load_rejects_bad_magic) {
  const std::string path = tmpPath("bad");
  PathGuard g{path};
  {
    std::ofstream out(path, std::ios::binary);
    const char junk[32] = "NOTAVALIDTUXELSFILE";
    out.write(junk, sizeof(junk));
  }
  std::string err;
  auto loaded = loadTxl(path, &err);
  CHECK(!loaded.has_value());
  CHECK(!err.empty());
}

TEST(txl_load_rejects_truncated_file) {
  const std::string path = tmpPath("trunc");
  PathGuard g{path};

  Document src(80, 80);
  auto* layer = src.addBlankPixelLayer("L");
  layer->image.fill(kBlue);

  std::string err;
  CHECK(saveTxl(path, src, &err));

  // Truncate the file to half size.
  const auto sz = std::filesystem::file_size(path);
  std::filesystem::resize_file(path, sz / 2);

  auto loaded = loadTxl(path, &err);
  CHECK(!loaded.has_value());
  CHECK(!err.empty());
}

TEST(txl_v3_round_trip_preserves_levels_adjustment) {
  const std::string path = tmpPath("levels");
  PathGuard g{path};

  Document src(80, 60);
  src.addBlankPixelLayer("BG");
  auto* lv = src.addAdjustmentLayer(std::make_unique<LevelsAdjustment>());
  lv->name = "Levels 1";
  lv->opacity = 0.6f;
  lv->blend = BlendMode::Multiply;
  lv->setParams(LevelsChannel::Composite,
                LevelsParams{0.05f, 0.95f, 1.8f, 0.02f, 0.98f});
  lv->setParams(LevelsChannel::R,
                LevelsParams{0.10f, 0.90f, 1.2f, 0.00f, 1.00f});
  lv->setParams(LevelsChannel::G,
                LevelsParams{0.00f, 1.00f, 0.8f, 0.05f, 0.95f});
  lv->setParams(LevelsChannel::B,
                LevelsParams{0.20f, 0.80f, 2.5f, 0.10f, 0.90f});

  std::string err;
  CHECK(saveTxl(path, src, &err));
  auto loaded = loadTxl(path, &err);
  CHECK(loaded.has_value());
  CHECK_EQ(static_cast<int>((*loaded)->tree().size()), 2);

  auto* loadedLv = dynamic_cast<LevelsAdjustment*>((*loaded)->tree().at(1));
  CHECK(loadedLv != nullptr);
  CHECK(loadedLv->name == "Levels 1");
  CHECK_NEAR(loadedLv->opacity, 0.6f, 1e-6f);
  CHECK(loadedLv->blend == BlendMode::Multiply);

  const auto& all = loadedLv->allParams();
  CHECK_NEAR(all[0].inBlack, 0.05f, 1e-6f);
  CHECK_NEAR(all[0].inWhite, 0.95f, 1e-6f);
  CHECK_NEAR(all[0].gamma, 1.8f, 1e-6f);
  CHECK_NEAR(all[0].outBlack, 0.02f, 1e-6f);
  CHECK_NEAR(all[0].outWhite, 0.98f, 1e-6f);
  CHECK_NEAR(all[1].inBlack, 0.10f, 1e-6f);
  CHECK_NEAR(all[2].gamma, 0.8f, 1e-6f);
  CHECK_NEAR(all[3].outWhite, 0.90f, 1e-6f);

  // addAdjustmentLayer auto-attaches a doc-sized white mask.
  CHECK(loadedLv->mask != nullptr);
  CHECK_EQ(loadedLv->mask->image.width(), 80);
  CHECK_EQ(loadedLv->mask->image.height(), 60);
}

TEST(txl_v3_round_trip_preserves_curves_adjustment) {
  const std::string path = tmpPath("curves");
  PathGuard g{path};

  Document src(64, 64);
  src.addBlankPixelLayer("BG");
  auto* cv = src.addAdjustmentLayer(std::make_unique<CurvesAdjustment>());
  cv->name = "Curves 1";

  // Composite: S-curve.
  cv->setPoints(CurvesChannel::Composite,
                {{0.f, 0.f}, {0.3f, 0.2f}, {0.7f, 0.8f}, {1.f, 1.f}});
  // R: single midpoint lift.
  cv->setPoints(CurvesChannel::R,
                {{0.f, 0.f}, {0.5f, 0.65f}, {1.f, 1.f}});
  // G: identity (default).
  cv->setPoints(CurvesChannel::G, {{0.f, 0.f}, {1.f, 1.f}});
  // B: five points including asymmetric endpoints.
  cv->setPoints(CurvesChannel::B,
                {{0.f, 0.05f},
                 {0.25f, 0.3f},
                 {0.5f, 0.55f},
                 {0.75f, 0.7f},
                 {1.f, 0.95f}});

  std::string err;
  CHECK(saveTxl(path, src, &err));
  auto loaded = loadTxl(path, &err);
  CHECK(loaded.has_value());

  auto* loadedCv = dynamic_cast<CurvesAdjustment*>((*loaded)->tree().at(1));
  CHECK(loadedCv != nullptr);
  CHECK(loadedCv->name == "Curves 1");

  const auto& comp = loadedCv->points(CurvesChannel::Composite);
  CHECK_EQ(static_cast<int>(comp.size()), 4);
  CHECK_NEAR(comp[1].x, 0.3f, 1e-6f);
  CHECK_NEAR(comp[1].y, 0.2f, 1e-6f);
  CHECK_NEAR(comp[2].x, 0.7f, 1e-6f);

  const auto& red = loadedCv->points(CurvesChannel::R);
  CHECK_EQ(static_cast<int>(red.size()), 3);
  CHECK_NEAR(red[1].y, 0.65f, 1e-6f);

  const auto& blue = loadedCv->points(CurvesChannel::B);
  CHECK_EQ(static_cast<int>(blue.size()), 5);
  CHECK_NEAR(blue[0].y, 0.05f, 1e-6f);
  CHECK_NEAR(blue[4].y, 0.95f, 1e-6f);
}

TEST(txl_v3_round_trip_preserves_mixed_layer_ordering) {
  // A doc with pixel + levels + curves layers interleaved. Order must
  // survive the round-trip, kind dispatch must pick the right subclass per
  // slot, and the adjustment masks must carry their custom content.
  const std::string path = tmpPath("mixed");
  PathGuard g{path};

  Document src(40, 40);
  auto* pxBottom = src.addBlankPixelLayer("Bottom");
  pxBottom->image.fill(kRed);
  auto* lv = src.addAdjustmentLayer(std::make_unique<LevelsAdjustment>());
  lv->name = "Levels Mid";
  lv->setParams(LevelsChannel::Composite,
                LevelsParams{0.0f, 1.0f, 2.0f, 0.0f, 1.0f});
  // Paint a half-doc mask on the adjustment so we verify mask round-trip.
  for (int y = 0; y < 40; ++y) {
    for (int x = 0; x < 20; ++x) {
      lv->mask->image.setPixel(x, y, Rgba32F{0.f, 0.f, 0.f, 1.f});
    }
  }
  auto* cv = src.addAdjustmentLayer(std::make_unique<CurvesAdjustment>());
  cv->name = "Curves Top";
  cv->setPoints(CurvesChannel::Composite,
                {{0.f, 0.f}, {0.5f, 0.7f}, {1.f, 1.f}});
  src.addBlankPixelLayer("Top Pixel");

  std::string err;
  CHECK(saveTxl(path, src, &err));
  auto loaded = loadTxl(path, &err);
  CHECK(loaded.has_value());

  auto& tree = (*loaded)->tree();
  CHECK_EQ(static_cast<int>(tree.size()), 4);
  CHECK(dynamic_cast<PixelLayer*>(tree.at(0)) != nullptr);
  CHECK(dynamic_cast<LevelsAdjustment*>(tree.at(1)) != nullptr);
  CHECK(dynamic_cast<CurvesAdjustment*>(tree.at(2)) != nullptr);
  CHECK(dynamic_cast<PixelLayer*>(tree.at(3)) != nullptr);
  CHECK(tree.at(1)->name == "Levels Mid");
  CHECK(tree.at(2)->name == "Curves Top");

  auto* loadedLv = static_cast<LevelsAdjustment*>(tree.at(1));
  CHECK_NEAR(loadedLv->allParams()[0].gamma, 2.0f, 1e-6f);
  CHECK(loadedLv->mask != nullptr);
  // Left-half mask pixels are zero; right-half pixels come back as 1.0
  // (white, the auto-mask default).
  CHECK_NEAR(loadedLv->mask->image.getPixel(10, 10).r, 0.f, 1e-6f);
  CHECK_NEAR(loadedLv->mask->image.getPixel(30, 10).r, 1.f, 1e-6f);
}

TEST(txl_v3_reader_accepts_v2_pixel_only_file) {
  // v2 files (no adjustment layers, kind byte always = 1) must load cleanly
  // in the v3-aware build. We synthesize a minimal v2 file by hand so the
  // test doesn't depend on the current writer continuing to emit v2.
  const std::string path = tmpPath("v2compat");
  PathGuard g{path};

  // Minimal header: no layers, no selection. Magic + version=2 + flags=0.
  {
    std::ofstream out(path, std::ios::binary);
    const char magic[8] = {'T', 'U', 'X', 'E', 'L', 'S', '\x01', '\x00'};
    out.write(magic, 8);
    const uint32_t version = 2;
    const uint32_t flags = 0;
    const uint32_t w = 32, h = 16;
    const int32_t activeLayer = -1;
    const uint8_t paintTarget = 0;
    const uint8_t hasSelection = 0;
    const uint16_t reserved = 0;
    const uint32_t numLayers = 0;
    out.write(reinterpret_cast<const char*>(&version), 4);
    out.write(reinterpret_cast<const char*>(&flags), 4);
    out.write(reinterpret_cast<const char*>(&w), 4);
    out.write(reinterpret_cast<const char*>(&h), 4);
    out.write(reinterpret_cast<const char*>(&activeLayer), 4);
    out.write(reinterpret_cast<const char*>(&paintTarget), 1);
    out.write(reinterpret_cast<const char*>(&hasSelection), 1);
    out.write(reinterpret_cast<const char*>(&reserved), 2);
    out.write(reinterpret_cast<const char*>(&numLayers), 4);
  }

  std::string err;
  auto loaded = loadTxl(path, &err);
  CHECK(loaded.has_value());
  CHECK_EQ((*loaded)->width(), 32);
  CHECK_EQ((*loaded)->height(), 16);
  CHECK_EQ(static_cast<int>((*loaded)->tree().size()), 0);
}

TEST(txl_v3_reader_rejects_unknown_kind_byte) {
  // Synthesize a v3 file with one layer whose kind byte is 99 (unknown).
  const std::string path = tmpPath("badkind");
  PathGuard g{path};
  {
    std::ofstream out(path, std::ios::binary);
    const char magic[8] = {'T', 'U', 'X', 'E', 'L', 'S', '\x01', '\x00'};
    out.write(magic, 8);
    auto wU32 = [&](uint32_t v) {
      out.write(reinterpret_cast<const char*>(&v), 4);
    };
    auto wU8 = [&](uint8_t v) {
      out.write(reinterpret_cast<const char*>(&v), 1);
    };
    auto wI32 = [&](int32_t v) {
      out.write(reinterpret_cast<const char*>(&v), 4);
    };
    wU32(3);                    // version
    wU32(0);                    // flags
    wU32(16); wU32(16);         // doc dims
    wI32(0);                    // active
    wU8(0); wU8(0);             // paintTarget, hasSelection
    uint16_t reserved = 0;
    out.write(reinterpret_cast<const char*>(&reserved), 2);
    wU32(1);                    // num layers
    // Layer header
    uint64_t id = 1;
    out.write(reinterpret_cast<const char*>(&id), 8);
    wU8(99);                    // kind = unknown
    wU8(1); wU8(0); wU8(0);     // visible/maskEnabled/hasMask
    float opacity = 1.f;
    out.write(reinterpret_cast<const char*>(&opacity), 4);
    wU32(0);                    // blend = Normal
    wU32(0); wU32(0);           // layerW/H
    wI32(0); wI32(0);           // originX/Y
    wU32(0);                    // nameLen
  }
  std::string err;
  auto loaded = loadTxl(path, &err);
  CHECK(!loaded.has_value());
  CHECK(!err.empty());
}

}  // namespace tuxels

int main() { return tuxels::testing::run(); }
