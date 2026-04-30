#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "core/Document.h"
#include "core/Pixel.h"
#include "core/TuxImage.h"
#include "io/PsdIO.h"
#include "layers/GroupLayer.h"
#include "layers/LayerMask.h"
#include "layers/PixelLayer.h"
#include "test_harness.h"

using namespace tuxels;

namespace {

// ---------- Tiny big-endian writer ----------
//
// Hand-rolling PSD bytes for tests is the only practical way — we can't
// commit binary fixtures and the format is too fiddly for ad-hoc memcpy.
class Builder {
 public:
  void u8(std::uint8_t v) { bytes_.push_back(v); }
  void u16(std::uint16_t v) {
    bytes_.push_back(static_cast<std::uint8_t>(v >> 8));
    bytes_.push_back(static_cast<std::uint8_t>(v & 0xff));
  }
  void s16(std::int16_t v) { u16(static_cast<std::uint16_t>(v)); }
  void u32(std::uint32_t v) {
    bytes_.push_back(static_cast<std::uint8_t>(v >> 24));
    bytes_.push_back(static_cast<std::uint8_t>((v >> 16) & 0xff));
    bytes_.push_back(static_cast<std::uint8_t>((v >> 8) & 0xff));
    bytes_.push_back(static_cast<std::uint8_t>(v & 0xff));
  }
  void s32(std::int32_t v) { u32(static_cast<std::uint32_t>(v)); }
  void raw(const void* data, std::size_t n) {
    const auto* p = static_cast<const std::uint8_t*>(data);
    bytes_.insert(bytes_.end(), p, p + n);
  }
  void str4(const char* s) { raw(s, 4); }

  // Emit a Pascal name padded to 4 bytes (counting the length byte).
  void pascalName4(const std::string& s) {
    const std::size_t len = s.size();
    bytes_.push_back(static_cast<std::uint8_t>(len));
    if (len > 0) raw(s.data(), len);
    const std::size_t total = 1 + len;
    const std::size_t pad = (4 - (total % 4)) % 4;
    for (std::size_t i = 0; i < pad; ++i) bytes_.push_back(0);
  }

  // Reserve a u32 length placeholder; returns a token for `patchU32`.
  std::size_t reserveU32() {
    const std::size_t at = bytes_.size();
    u32(0);
    return at;
  }
  // Patch a previously-reserved u32 with the bytes written since.
  void patchU32WithGap(std::size_t at, std::uint32_t v) {
    bytes_[at + 0] = static_cast<std::uint8_t>(v >> 24);
    bytes_[at + 1] = static_cast<std::uint8_t>((v >> 16) & 0xff);
    bytes_[at + 2] = static_cast<std::uint8_t>((v >> 8) & 0xff);
    bytes_[at + 3] = static_cast<std::uint8_t>(v & 0xff);
  }
  // Convenience: backfill `at` so the value at that slot equals "bytes
  // written between (at + 4) and now".
  void backfillLength(std::size_t at) {
    const std::uint32_t len =
        static_cast<std::uint32_t>(bytes_.size() - (at + 4));
    patchU32WithGap(at, len);
  }

  std::span<const std::uint8_t> view() const {
    return {bytes_.data(), bytes_.size()};
  }
  std::size_t size() const { return bytes_.size(); }

 private:
  std::vector<std::uint8_t> bytes_;
};

// PSD header for an 8-bit RGB doc with `channels` channels (3 = RGB,
// 4 = RGBA).
void writeHeader(Builder& b, std::uint16_t channels, std::uint32_t w,
                  std::uint32_t h) {
  b.str4("8BPS");
  b.u16(1);  // version 1 (PSD, not PSB)
  for (int i = 0; i < 6; ++i) b.u8(0);  // reserved
  b.u16(channels);
  b.u32(h);
  b.u32(w);
  b.u16(8);  // depth
  b.u16(3);  // mode = RGB
}

// One pixel layer record. Channel data is raw (compression == 0). The
// caller passes per-channel byte buffers (R, G, B, A) of size lw*lh
// each. blendKey is a 4-char string; opacity 0-255; flags bit 1 = hidden.
struct LayerRecArgs {
  std::int32_t top, left, bottom, right;
  std::vector<std::uint8_t> r, g, b, a;
  const char* blendKey = "norm";
  std::uint8_t opacity = 255;
  std::uint8_t flags = 0;
  std::string name;
  // Optional section divider override.
  std::uint32_t sectionDividerType = 0;
  const char* sectionDividerBlend = "pass";
  // Optional layer mask: rect + per-pixel buffer (size mw*mh).
  bool hasMask = false;
  std::int32_t maskTop = 0, maskLeft = 0, maskBottom = 0, maskRight = 0;
  std::vector<std::uint8_t> maskData;
};

// Write one layer's record (header + extra-data block). Channel image
// data is written separately via writeLayerChannelDataRaw.
void writeLayerRecord(Builder& b, const LayerRecArgs& a) {
  b.s32(a.top);
  b.s32(a.left);
  b.s32(a.bottom);
  b.s32(a.right);
  // Channels: R, G, B, A → ids 0, 1, 2, -1. + mask channel if present.
  std::uint16_t channelCount = 4;
  if (a.hasMask) channelCount += 1;  // user mask = -2
  b.u16(channelCount);
  // (channelId, dataLength) per channel. dataLength = compression word
  // (2 bytes, raw=0) + width*height bytes.
  const std::int32_t lw = a.right - a.left;
  const std::int32_t lh = a.bottom - a.top;
  const std::uint32_t pixelLen = 2 + static_cast<std::uint32_t>(lw) * lh;
  b.s16(0);
  b.u32(pixelLen);
  b.s16(1);
  b.u32(pixelLen);
  b.s16(2);
  b.u32(pixelLen);
  b.s16(-1);
  b.u32(pixelLen);
  if (a.hasMask) {
    const std::int32_t mw = a.maskRight - a.maskLeft;
    const std::int32_t mh = a.maskBottom - a.maskTop;
    const std::uint32_t maskLen =
        2 + static_cast<std::uint32_t>(mw) * mh;
    b.s16(-2);
    b.u32(maskLen);
  }

  b.str4("8BIM");
  b.raw(a.blendKey, 4);
  b.u8(a.opacity);
  b.u8(0);  // clipping
  b.u8(a.flags);
  b.u8(0);  // filler

  // Extra data length (placeholder).
  const std::size_t extraStart = b.reserveU32();

  // Layer mask data block.
  if (a.hasMask) {
    const std::size_t maskBlockStart = b.reserveU32();
    b.s32(a.maskTop);
    b.s32(a.maskLeft);
    b.s32(a.maskBottom);
    b.s32(a.maskRight);
    b.u8(0);  // default
    b.u8(0);  // flags
    b.u16(0);  // padding
    b.backfillLength(maskBlockStart);
  } else {
    b.u32(0);
  }

  // Layer blending ranges (empty).
  b.u32(0);

  // Layer name (Pascal-padded).
  b.pascalName4(a.name);

  // Section divider additional layer info, if requested.
  if (a.sectionDividerType != 0) {
    b.str4("8BIM");
    b.str4("lsct");
    const std::size_t lsctLenAt = b.reserveU32();
    b.u32(a.sectionDividerType);
    b.str4("8BIM");
    b.raw(a.sectionDividerBlend, 4);
    b.backfillLength(lsctLenAt);
    // Block padded to 2-byte boundary; lsct length is a multiple of 4
    // here so no padding needed.
  }

  // Backfill extra-data length.
  b.backfillLength(extraStart);
}

// Write one layer's channel image data (per the order R, G, B, A,
// optionally mask). Compression word = 0 (raw).
void writeLayerChannelDataRaw(Builder& b, const LayerRecArgs& a) {
  const std::int32_t lw = a.right - a.left;
  const std::int32_t lh = a.bottom - a.top;
  const std::size_t n = static_cast<std::size_t>(lw) * lh;
  for (const auto* ch : {&a.r, &a.g, &a.b, &a.a}) {
    b.u16(0);  // compression = raw
    if (ch->size() == n) {
      b.raw(ch->data(), n);
    } else {
      // Padded zero buffer for a missing channel.
      for (std::size_t i = 0; i < n; ++i) b.u8(0);
    }
  }
  if (a.hasMask) {
    b.u16(0);
    b.raw(a.maskData.data(), a.maskData.size());
  }
}

// Top-level: build a complete PSD file with the given layer list. Doc
// dims are taken from the bbox-union of the records.
std::vector<std::uint8_t> buildPsd(const std::vector<LayerRecArgs>& layers,
                                    int docW, int docH) {
  Builder b;
  writeHeader(b, /*channels=*/4, static_cast<std::uint32_t>(docW),
               static_cast<std::uint32_t>(docH));
  // Color Mode Data — empty for RGB.
  b.u32(0);
  // Image Resources — empty.
  b.u32(0);

  // Layer and Mask Info section.
  const std::size_t lmiStart = b.reserveU32();
  // Layer Info subsection.
  const std::size_t liStart = b.reserveU32();
  b.s16(static_cast<std::int16_t>(layers.size()));
  for (const auto& l : layers) writeLayerRecord(b, l);
  for (const auto& l : layers) writeLayerChannelDataRaw(b, l);
  b.backfillLength(liStart);
  // Pad layer-info subsection to 2 bytes.
  if ((b.size() - (liStart + 4)) % 2 != 0) b.u8(0);
  // Global mask info — empty.
  b.u32(0);
  // No additional info blocks.
  b.backfillLength(lmiStart);

  // Merged image data section (compression = 0, then docW*docH*channels
  // zero bytes — we don't emit a real composite for the test).
  b.u16(0);
  for (int i = 0; i < docW * docH * 4; ++i) b.u8(0);
  return std::vector<std::uint8_t>(b.view().begin(), b.view().end());
}

}  // namespace

// ---------- Header validation ----------

TEST(psd_rejects_bad_magic) {
  std::vector<std::uint8_t> bytes(64, 0);
  std::memcpy(bytes.data(), "NOTPSD!!", 4);
  std::string err;
  auto doc = loadPsdBytes(std::span<const std::uint8_t>(bytes), &err);
  CHECK(!doc.has_value());
  CHECK(!err.empty());
}

TEST(psd_rejects_psb_version) {
  Builder b;
  b.str4("8BPS");
  b.u16(2);  // version 2 = PSB
  for (int i = 0; i < 20; ++i) b.u8(0);
  std::string err;
  auto doc = loadPsdBytes(b.view(), &err);
  CHECK(!doc.has_value());
}

TEST(psd_rejects_non_8bit_depth) {
  Builder b;
  writeHeader(b, 3, 8, 8);
  // Stomp depth byte to 16. The header writes depth at offset 26-27 +
  // (4 magic + 2 ver + 6 reserved + 2 ch + 4 h + 4 w) = 22; at byte 22
  // begins depth. Easier to just rebuild manually.
  b = Builder{};
  b.str4("8BPS");
  b.u16(1);
  for (int i = 0; i < 6; ++i) b.u8(0);
  b.u16(3);
  b.u32(8);
  b.u32(8);
  b.u16(16);  // 16-bit depth
  b.u16(3);
  std::string err;
  auto doc = loadPsdBytes(b.view(), &err);
  CHECK(!doc.has_value());
}

// ---------- Single-layer flat raw RGB ----------

TEST(psd_loads_single_pixel_layer_raw) {
  // 4x4 layer, all-red (R=255, G=0, B=0, A=255).
  std::vector<std::uint8_t> r(16, 255), g(16, 0), bch(16, 0), a(16, 255);
  LayerRecArgs l;
  l.top = 0; l.left = 0; l.bottom = 4; l.right = 4;
  l.r = r; l.g = g; l.b = bch; l.a = a;
  l.name = "L";
  auto bytes = buildPsd({l}, 4, 4);

  std::string err;
  auto doc = loadPsdBytes(std::span<const std::uint8_t>(bytes), &err);
  CHECK(doc.has_value());
  if (!doc.has_value()) return;
  Document* d = doc->get();
  CHECK_EQ(d->width(), 4);
  CHECK_EQ(d->height(), 4);
  CHECK_EQ(static_cast<int>(d->tree().size()), 1);
  auto* px = dynamic_cast<PixelLayer*>(d->tree().at(0));
  CHECK(px != nullptr);
  CHECK(px->name == "L");
  CHECK_EQ(px->image.width(), 4);
  CHECK_EQ(px->image.height(), 4);
  Rgba32F p = px->image.getPixel(2, 2);
  CHECK_NEAR(p.r, 1.f, 1e-3);
  CHECK_NEAR(p.g, 0.f, 1e-3);
  CHECK_NEAR(p.b, 0.f, 1e-3);
  CHECK_NEAR(p.a, 1.f, 1e-3);
}

TEST(psd_two_layers_inherit_blend_and_opacity) {
  std::vector<std::uint8_t> z(16, 0), full(16, 255);
  LayerRecArgs l1;
  l1.top = 0; l1.left = 0; l1.bottom = 4; l1.right = 4;
  l1.r = full; l1.g = z; l1.b = z; l1.a = full;
  l1.name = "Bottom";
  l1.blendKey = "norm";
  l1.opacity = 255;

  LayerRecArgs l2;
  l2.top = 0; l2.left = 0; l2.bottom = 4; l2.right = 4;
  l2.r = z; l2.g = full; l2.b = z; l2.a = full;
  l2.name = "Top";
  l2.blendKey = "mul ";
  l2.opacity = 128;
  auto bytes = buildPsd({l1, l2}, 4, 4);

  std::string err;
  auto doc = loadPsdBytes(std::span<const std::uint8_t>(bytes), &err);
  CHECK(doc.has_value());
  if (!doc.has_value()) return;
  Document* d = doc->get();
  CHECK_EQ(static_cast<int>(d->tree().size()), 2);
  auto* bottom = dynamic_cast<PixelLayer*>(d->tree().at(0));
  auto* top = dynamic_cast<PixelLayer*>(d->tree().at(1));
  CHECK(bottom != nullptr && top != nullptr);
  CHECK(bottom->name == "Bottom");
  CHECK(top->name == "Top");
  CHECK(top->blend == BlendMode::Multiply);
  CHECK_NEAR(top->opacity, 128.f / 255.f, 1e-4);
}

TEST(psd_section_dividers_build_group_tree) {
  // PSD enumerates groups bottom-to-top. To build [Group{A, B}] we
  // emit: bounding-divider record (closes group), B, A, open-folder
  // record (named "G"). The reader pushes a GroupLayer when it sees
  // the open-folder marker and pops on the bounding divider.
  std::vector<std::uint8_t> z(16, 0), full(16, 255);

  // A bounding-divider record (sectionDividerType == 3) sits at the
  // bottom of the group's contents.
  LayerRecArgs bound;
  bound.top = 0; bound.left = 0; bound.bottom = 4; bound.right = 4;
  bound.r = z; bound.g = z; bound.b = z; bound.a = z;
  bound.name = "</Group>";
  bound.sectionDividerType = 3;

  LayerRecArgs a;
  a.top = 0; a.left = 0; a.bottom = 4; a.right = 4;
  a.r = full; a.g = z; a.b = z; a.a = full;
  a.name = "A";

  LayerRecArgs b;
  b.top = 0; b.left = 0; b.bottom = 4; b.right = 4;
  b.r = z; b.g = full; b.b = z; b.a = full;
  b.name = "B";

  LayerRecArgs open;
  open.top = 0; open.left = 0; open.bottom = 4; open.right = 4;
  open.r = z; open.g = z; open.b = z; open.a = z;
  open.name = "G";
  open.sectionDividerType = 1;

  auto bytes = buildPsd({bound, a, b, open}, 4, 4);
  std::string err;
  auto doc = loadPsdBytes(std::span<const std::uint8_t>(bytes), &err);
  CHECK(doc.has_value());
  if (!doc.has_value()) return;
  Document* d = doc->get();
  CHECK_EQ(static_cast<int>(d->tree().size()), 1);
  auto* g = dynamic_cast<GroupLayer*>(d->tree().at(0));
  CHECK(g != nullptr);
  if (!g) return;
  CHECK(g->name == "G");
  CHECK_EQ(static_cast<int>(g->children.size()), 2);
}

TEST(psd_layer_mask_round_trip) {
  std::vector<std::uint8_t> r(16, 255), g(16, 0), bch(16, 0), a(16, 255);
  std::vector<std::uint8_t> mask(16);
  for (int i = 0; i < 16; ++i) mask[i] = static_cast<std::uint8_t>(i * 16);

  LayerRecArgs l;
  l.top = 0; l.left = 0; l.bottom = 4; l.right = 4;
  l.r = r; l.g = g; l.b = bch; l.a = a;
  l.name = "M";
  l.hasMask = true;
  l.maskTop = 0; l.maskLeft = 0; l.maskBottom = 4; l.maskRight = 4;
  l.maskData = mask;

  auto bytes = buildPsd({l}, 4, 4);
  std::string err;
  auto doc = loadPsdBytes(std::span<const std::uint8_t>(bytes), &err);
  CHECK(doc.has_value());
  if (!doc.has_value()) return;
  Document* d = doc->get();
  auto* px = dynamic_cast<PixelLayer*>(d->tree().at(0));
  CHECK(px != nullptr);
  CHECK(px->mask != nullptr);
  if (px->mask) {
    Rgba32F m = px->mask->image.getPixel(0, 0);
    CHECK_NEAR(m.r, 0.f, 1e-3);
    Rgba32F m2 = px->mask->image.getPixel(3, 3);
    CHECK_NEAR(m2.r, 240.f / 255.f, 1e-3);
  }
}

int main() { return tuxels::testing::run(); }
