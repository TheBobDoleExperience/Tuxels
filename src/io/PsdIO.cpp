#include "io/PsdIO.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <vector>

#include "compositor/BlendMode.h"
#include "core/Document.h"
#include "core/TuxImage.h"
#include "layers/GroupLayer.h"
#include "layers/LayerMask.h"
#include "layers/PixelLayer.h"

namespace tuxels {

namespace {

// ---------- Big-endian readers + bounds-checked cursor ----------
//
// PSD stores all integers in big-endian order. The Cursor wraps a
// span of bytes plus a read offset; every read advances the offset and
// fails fast (sets `failed_`) if there aren't enough bytes left.

class Cursor {
 public:
  explicit Cursor(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

  bool ok() const noexcept { return !failed_; }
  std::size_t pos() const noexcept { return pos_; }
  std::size_t remaining() const noexcept {
    return failed_ ? 0 : bytes_.size() - pos_;
  }

  void fail() noexcept { failed_ = true; }
  void seek(std::size_t p) noexcept {
    if (p > bytes_.size()) {
      failed_ = true;
      return;
    }
    pos_ = p;
  }
  void skip(std::size_t n) noexcept {
    if (failed_) return;
    if (n > bytes_.size() - pos_) {
      failed_ = true;
      return;
    }
    pos_ += n;
  }

  std::uint8_t u8() noexcept {
    if (failed_) return 0;
    if (pos_ >= bytes_.size()) {
      failed_ = true;
      return 0;
    }
    return bytes_[pos_++];
  }
  std::uint16_t u16() noexcept {
    const std::uint8_t a = u8();
    const std::uint8_t b = u8();
    return static_cast<std::uint16_t>((a << 8) | b);
  }
  std::int16_t s16() noexcept {
    return static_cast<std::int16_t>(u16());
  }
  std::uint32_t u32() noexcept {
    const std::uint8_t a = u8();
    const std::uint8_t b = u8();
    const std::uint8_t c = u8();
    const std::uint8_t d = u8();
    return (static_cast<std::uint32_t>(a) << 24) |
           (static_cast<std::uint32_t>(b) << 16) |
           (static_cast<std::uint32_t>(c) << 8) | static_cast<std::uint32_t>(d);
  }
  std::int32_t s32() noexcept { return static_cast<std::int32_t>(u32()); }

  // Copy `n` raw bytes into `dst`. Caller-supplied buffer of size n.
  void readRaw(std::uint8_t* dst, std::size_t n) noexcept {
    if (failed_) return;
    if (n > bytes_.size() - pos_) {
      failed_ = true;
      return;
    }
    std::memcpy(dst, bytes_.data() + pos_, n);
    pos_ += n;
  }

 private:
  std::span<const std::uint8_t> bytes_;
  std::size_t pos_ = 0;
  bool failed_ = false;
};

void setErr(std::string* err, const std::string& msg) {
  if (err) *err = msg;
}

// ---------- Blend mode mapping ----------
//
// PSD blend mode keys are 4-byte ASCII tags inside the layer record. We
// map the ones the local engine implements; unknown keys fall back to
// Normal so the layer at least appears.
BlendMode psdBlendKeyToLocal(const std::array<std::uint8_t, 4>& key) {
  // Treat the 4 chars as a packed uint32 for fast equality.
  auto pack = [](const char* k) -> std::uint32_t {
    return (static_cast<std::uint32_t>(static_cast<std::uint8_t>(k[0])) << 24) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(k[1])) << 16) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(k[2])) << 8) |
           static_cast<std::uint32_t>(static_cast<std::uint8_t>(k[3]));
  };
  const std::uint32_t k =
      (static_cast<std::uint32_t>(key[0]) << 24) |
      (static_cast<std::uint32_t>(key[1]) << 16) |
      (static_cast<std::uint32_t>(key[2]) << 8) |
      static_cast<std::uint32_t>(key[3]);
  if (k == pack("norm")) return BlendMode::Normal;
  if (k == pack("diss")) return BlendMode::Dissolve;
  if (k == pack("mul ") || k == pack("mlti")) return BlendMode::Multiply;
  if (k == pack("scrn")) return BlendMode::Screen;
  if (k == pack("over")) return BlendMode::Overlay;
  if (k == pack("dark")) return BlendMode::Darken;
  if (k == pack("lite") || k == pack("lght")) return BlendMode::Lighten;
  if (k == pack("idiv") || k == pack("div ")) return BlendMode::ColorBurn;
  if (k == pack("lddg") || k == pack("lDdg")) return BlendMode::ColorDodge;
  if (k == pack("sLit") || k == pack("sft ")) return BlendMode::SoftLight;
  if (k == pack("hLit")) return BlendMode::HardLight;
  if (k == pack("diff")) return BlendMode::Difference;
  if (k == pack("smud")) return BlendMode::Exclusion;
  if (k == pack("pass")) return BlendMode::PassThrough;
  return BlendMode::Normal;
}

// ---------- PackBits (RLE) decoder ----------
//
// Per Apple's PackBits spec: a control byte n tells us how to read the
// next chunk:
//   n in [0, 127]    → copy the next (n + 1) bytes literally.
//   n in [-127, -1]  → repeat the next single byte (1 - n) times.
//   n == -128        → no-op.
// The decoder consumes from `src` and writes exactly `expectedBytes` to
// `dst`. Returns true on success.
bool packBitsDecode(Cursor& cur, std::uint8_t* dst, std::size_t expected) {
  std::size_t out = 0;
  while (out < expected && cur.ok()) {
    const std::int8_t n = static_cast<std::int8_t>(cur.u8());
    if (!cur.ok()) return false;
    if (n == -128) continue;
    if (n >= 0) {
      const std::size_t count = static_cast<std::size_t>(n) + 1;
      if (out + count > expected) return false;
      cur.readRaw(dst + out, count);
      out += count;
    } else {
      const std::size_t count = static_cast<std::size_t>(1 - n);
      if (out + count > expected) return false;
      const std::uint8_t value = cur.u8();
      for (std::size_t i = 0; i < count; ++i) dst[out + i] = value;
      out += count;
    }
  }
  return cur.ok() && out == expected;
}

// ---------- Per-channel image data ----------
//
// PSD per-layer channel layout: u16 compression, then either raw bytes
// or RLE-packed rows. RLE format starts with `height` u16 row byte-counts
// (one per row) followed by the concatenated PackBits payload.
//
// `out` is a single-channel byte buffer of size width * height. Returns
// true on success; sets `err` on failure.
bool decodeChannel(Cursor& cur, int width, int height, std::uint8_t* out,
                    std::string* err) {
  if (width < 0 || height < 0) {
    setErr(err, "Negative channel dimensions");
    return false;
  }
  const std::uint16_t compression = cur.u16();
  if (!cur.ok()) {
    setErr(err, "Truncated channel: missing compression word");
    return false;
  }
  const std::size_t pixelCount =
      static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

  if (compression == 0) {  // raw
    cur.readRaw(out, pixelCount);
    if (!cur.ok()) {
      setErr(err, "Truncated raw channel data");
      return false;
    }
    return true;
  }
  if (compression == 1) {  // PackBits RLE
    // Skip the per-row byte-count table (we don't need it for streaming
    // decode; PackBits is self-delimiting given a target output size).
    cur.skip(static_cast<std::size_t>(height) * 2);
    if (!cur.ok()) {
      setErr(err, "Truncated RLE row-count table");
      return false;
    }
    // Decode row-by-row so a single bad row's overrun doesn't trash
    // subsequent rows.
    for (int y = 0; y < height; ++y) {
      if (!packBitsDecode(cur, out + static_cast<std::size_t>(y) * width,
                           static_cast<std::size_t>(width))) {
        setErr(err, "PackBits decode failed");
        return false;
      }
    }
    return true;
  }
  setErr(err, "Unsupported compression: " + std::to_string(compression));
  return false;
}

// ---------- Header ----------

struct Header {
  std::uint16_t version;
  std::uint16_t channels;
  std::uint32_t height;
  std::uint32_t width;
  std::uint16_t depth;
  std::uint16_t colorMode;
};

bool readHeader(Cursor& cur, Header& h, std::string* err) {
  std::uint8_t magic[4];
  cur.readRaw(magic, 4);
  if (!cur.ok() || magic[0] != '8' || magic[1] != 'B' || magic[2] != 'P' ||
      magic[3] != 'S') {
    setErr(err, "Not a PSD file (missing 8BPS magic)");
    return false;
  }
  h.version = cur.u16();
  if (h.version != 1) {
    setErr(err, "Unsupported PSD version (PSB / large-document files are "
                "not supported in M12)");
    return false;
  }
  cur.skip(6);  // 6 reserved zero bytes
  h.channels = cur.u16();
  h.height = cur.u32();
  h.width = cur.u32();
  h.depth = cur.u16();
  h.colorMode = cur.u16();
  if (!cur.ok()) {
    setErr(err, "Truncated PSD header");
    return false;
  }
  if (h.depth != 8) {
    setErr(err, "Only 8-bit-per-channel PSDs supported (file is " +
                    std::to_string(h.depth) + "-bit)");
    return false;
  }
  if (h.colorMode != 3) {
    setErr(err, "Only RGB color mode supported in M12 (file mode = " +
                    std::to_string(h.colorMode) + ")");
    return false;
  }
  if (h.channels < 1 || h.channels > 56) {
    setErr(err, "Invalid channel count: " + std::to_string(h.channels));
    return false;
  }
  return true;
}

// ---------- Per-layer record ----------

struct LayerRecord {
  std::int32_t top, left, bottom, right;
  std::uint16_t channelCount;
  // (channelId, dataLength) — channelId is signed, -1 = transparency
  // mask, -2 = user mask, -3 = real (vector) mask. Non-negative = R/G/B.
  std::vector<std::pair<std::int16_t, std::uint32_t>> channels;
  BlendMode blend = BlendMode::Normal;
  std::uint8_t opacity = 255;
  std::uint8_t clipping = 0;
  std::uint8_t flags = 0;
  std::string name;
  // M12-S3: section divider sub-block (`lsct`) detected in additional
  // layer info. 0 = not a divider, 1 = open folder, 2 = closed folder,
  // 3 = bounding section divider (closes a group). Plus the inner blend
  // mode key recorded in the lsct payload (PSD encodes the group's blend
  // there rather than in the main header).
  std::uint32_t sectionDividerType = 0;
  BlendMode sectionDividerBlend = BlendMode::PassThrough;
  // M12-S4: per-layer mask metadata (top/left/bottom/right + default).
  bool hasMask = false;
  std::int32_t maskTop = 0, maskLeft = 0, maskBottom = 0, maskRight = 0;
  std::uint8_t maskDefault = 0;
  std::uint8_t maskFlags = 0;
};

}  // namespace

// Forward declarations for the implementation that follows in another
// step's commit (M12-S2..S4). M12-S0 only provides the parser skeleton +
// header parsing + a single-layer flat-RGB raw decode path; subsequent
// steps fill in multi-layer / RLE / sections / masks.

namespace {

// Small helper: build a TuxImage from byte channels (R, G, B, optional A)
// — each channel is a width*height byte buffer. Alpha defaults to 255
// when absent.
TuxImage buildRGBAImage(int w, int h, const std::uint8_t* r,
                         const std::uint8_t* g, const std::uint8_t* b,
                         const std::uint8_t* a) {
  TuxImage img(w, h);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const std::size_t idx = static_cast<std::size_t>(y) * w + x;
      Rgba32F c;
      c.r = r ? r[idx] / 255.f : 0.f;
      c.g = g ? g[idx] / 255.f : 0.f;
      c.b = b ? b[idx] / 255.f : 0.f;
      c.a = a ? a[idx] / 255.f : 1.f;
      img.setPixel(x, y, c);
    }
  }
  return img;
}

// Parse a single layer record's metadata. Channel image data follows in
// a separate pass (after all layer records). Returns false on truncated
// / malformed input.
bool readLayerRecord(Cursor& cur, LayerRecord& rec, std::string* err) {
  rec.top = cur.s32();
  rec.left = cur.s32();
  rec.bottom = cur.s32();
  rec.right = cur.s32();
  rec.channelCount = cur.u16();
  if (!cur.ok()) {
    setErr(err, "Truncated layer record header");
    return false;
  }
  rec.channels.reserve(rec.channelCount);
  for (std::uint16_t i = 0; i < rec.channelCount; ++i) {
    const std::int16_t cid = cur.s16();
    const std::uint32_t len = cur.u32();
    rec.channels.emplace_back(cid, len);
  }
  std::uint8_t bsig[4];
  cur.readRaw(bsig, 4);
  if (!cur.ok() || bsig[0] != '8' || bsig[1] != 'B' || bsig[2] != 'I' ||
      bsig[3] != 'M') {
    setErr(err, "Layer record missing 8BIM blend-mode signature");
    return false;
  }
  std::array<std::uint8_t, 4> bkey;
  cur.readRaw(bkey.data(), 4);
  rec.blend = psdBlendKeyToLocal(bkey);
  rec.opacity = cur.u8();
  rec.clipping = cur.u8();
  rec.flags = cur.u8();
  cur.skip(1);  // filler

  const std::uint32_t extraLen = cur.u32();
  if (!cur.ok()) {
    setErr(err, "Truncated layer record extra-data length");
    return false;
  }
  const std::size_t extraStart = cur.pos();
  const std::size_t extraEnd = extraStart + extraLen;

  // Layer mask data (M12-S4).
  const std::uint32_t maskLen = cur.u32();
  if (maskLen > 0) {
    const std::size_t maskStart = cur.pos();
    rec.hasMask = true;
    rec.maskTop = cur.s32();
    rec.maskLeft = cur.s32();
    rec.maskBottom = cur.s32();
    rec.maskRight = cur.s32();
    rec.maskDefault = cur.u8();
    rec.maskFlags = cur.u8();
    // Skip remaining mask data (padding, real mask params, …).
    cur.seek(maskStart + maskLen);
  }

  // Layer blending ranges — variable length, we don't interpret. Skip.
  const std::uint32_t blendingLen = cur.u32();
  cur.skip(blendingLen);

  // Layer name — Pascal string, padded to a multiple of 4. Length byte
  // counts the *string* bytes only, not the padding.
  const std::uint8_t nameLen = cur.u8();
  if (cur.ok() && nameLen > 0) {
    std::vector<std::uint8_t> nb(nameLen);
    cur.readRaw(nb.data(), nameLen);
    rec.name.assign(reinterpret_cast<const char*>(nb.data()), nameLen);
  }
  // The Pascal-padded total (1 + nameLen + padding) must round up to 4.
  const std::size_t pascalTotal = 1 + nameLen;
  const std::size_t padding = (4 - (pascalTotal % 4)) % 4;
  cur.skip(padding);

  // M12-S3: scan additional layer info blocks looking for `lsct` (section
  // divider) and `luni` (unicode name override). Each block: signature
  // (4 bytes, "8BIM" or "8B64"), key (4 bytes), length (u32 for v1),
  // payload.
  while (cur.ok() && cur.pos() < extraEnd) {
    std::uint8_t sig[4];
    cur.readRaw(sig, 4);
    if (!cur.ok()) break;
    const bool ok8BIM = (sig[0] == '8' && sig[1] == 'B' && sig[2] == 'I' &&
                          sig[3] == 'M');
    const bool ok8B64 = (sig[0] == '8' && sig[1] == 'B' && sig[2] == '6' &&
                          sig[3] == '4');
    if (!ok8BIM && !ok8B64) break;  // misaligned — bail
    std::uint8_t key[4];
    cur.readRaw(key, 4);
    const std::uint32_t blen = cur.u32();
    const std::size_t blockStart = cur.pos();
    if (key[0] == 'l' && key[1] == 's' && key[2] == 'c' && key[3] == 't') {
      // Section divider. Layout: u32 type, optional 4-byte 8BIM signature
      // + 4-byte blend key, optional u32 sub-type.
      rec.sectionDividerType = cur.u32();
      if (blen >= 12) {
        std::uint8_t sigB[4];
        cur.readRaw(sigB, 4);
        std::array<std::uint8_t, 4> bk;
        cur.readRaw(bk.data(), 4);
        rec.sectionDividerBlend = psdBlendKeyToLocal(bk);
      }
    } else if (key[0] == 'l' && key[1] == 'u' && key[2] == 'n' &&
               key[3] == 'i') {
      // Unicode name override (UTF-16 BE). Layout: u32 char count, then
      // 2*count bytes of UTF-16 BE. We decode ASCII subset into UTF-8;
      // non-ASCII chars are replaced with '?' (sufficient for naming
      // round-trip in M12).
      const std::uint32_t chCount = cur.u32();
      std::string u;
      u.reserve(chCount);
      for (std::uint32_t i = 0; i < chCount; ++i) {
        const std::uint16_t cp = cur.u16();
        if (cp == 0) continue;  // null padding
        if (cp < 0x80) {
          u.push_back(static_cast<char>(cp));
        } else {
          u.push_back('?');
        }
      }
      if (!u.empty()) rec.name = std::move(u);
    }
    // Skip to the end of this block — pad to 2 bytes per the PSD spec.
    cur.seek(blockStart + blen);
    if ((blen & 1u) != 0) cur.skip(1);
  }

  // Defensive: jump past any unconsumed extra-data region.
  cur.seek(extraEnd);
  return cur.ok();
}

// Parse the layer-and-mask info section's layer-info subsection,
// populating `records` and (after) decoding each layer's channel image
// data into per-channel byte buffers (built into PixelLayer images).
//
// Returns true on success; populates `outDoc.tree()` with the parsed
// PixelLayers + GroupLayer wrappers. Group nesting is reconstructed from
// the section-divider records.
bool readLayerInfo(Cursor& cur, Document& outDoc, int docW, int docH,
                    std::string* err) {
  // M12-S0/S2/S3: layer-info section.
  //
  // Layout: u32 layerInfoLen, s16 layerCount (negative magnitude =
  // alpha-first / no-merged-alpha hint, we treat it as count |x|), then
  // layerCount records, then channel image data, padded to 2-byte
  // boundary at the end.
  const std::uint32_t layerInfoLen = cur.u32();
  if (!cur.ok()) {
    setErr(err, "Truncated layer-info length");
    return false;
  }
  const std::size_t layerInfoStart = cur.pos();
  const std::size_t layerInfoEnd = layerInfoStart + layerInfoLen;
  if (layerInfoLen == 0) {
    // Empty layer info (the doc has only a flat composite). The caller
    // can still load the merged image data section if it wants — for
    // M12 we leave the doc empty in this case so the user sees that the
    // file had no layers.
    cur.seek(layerInfoEnd);
    return true;
  }
  const std::int16_t signedCount = cur.s16();
  const std::uint16_t layerCount =
      static_cast<std::uint16_t>(signedCount < 0 ? -signedCount : signedCount);
  if (!cur.ok()) {
    setErr(err, "Truncated layer count");
    return false;
  }

  std::vector<LayerRecord> recs(layerCount);
  for (std::uint16_t i = 0; i < layerCount; ++i) {
    if (!readLayerRecord(cur, recs[i], err)) return false;
  }

  // Parse channel image data (one block per layer, per channel, in the
  // order defined by the records). Each block starts with a u16
  // compression, then channel data sized layerHeight * layerWidth.
  std::vector<TuxImage> layerImages(layerCount);
  std::vector<std::vector<std::uint8_t>> layerMasks(layerCount);
  std::vector<int> maskWs(layerCount), maskHs(layerCount);
  for (std::uint16_t i = 0; i < layerCount; ++i) {
    const auto& r = recs[i];
    const int lw = std::max(0, r.right - r.left);
    const int lh = std::max(0, r.bottom - r.top);
    std::vector<std::uint8_t> cR, cG, cB, cA;
    std::vector<std::uint8_t> cMask;
    int maskW = 0, maskH = 0;
    if (lw > 0 && lh > 0) {
      cR.resize(static_cast<std::size_t>(lw) * lh);
      cG.resize(static_cast<std::size_t>(lw) * lh);
      cB.resize(static_cast<std::size_t>(lw) * lh);
      cA.resize(static_cast<std::size_t>(lw) * lh, 255);
    }
    bool haveR = false, haveG = false, haveB = false, haveA = false;
    for (const auto& [cid, /*len*/ _] : r.channels) {
      (void)_;
      // Channel id 0/1/2 = R/G/B; -1 = alpha; -2 = user mask;
      // -3 = real (vector) mask. We honor 0/1/2/-1/-2.
      if (cid == 0 || cid == 1 || cid == 2 || cid == -1) {
        if (lw == 0 || lh == 0) {
          // Decode into a throwaway buffer to advance the cursor past
          // the empty-rect channel data (still has a compression word).
          std::uint8_t tmp = 0;
          (void)tmp;
          // Just consume the compression word + skip per spec: empty
          // rect → no bytes after the u16.
          cur.skip(2);
          continue;
        }
        std::uint8_t* dst = nullptr;
        if (cid == 0) { dst = cR.data(); haveR = true; }
        else if (cid == 1) { dst = cG.data(); haveG = true; }
        else if (cid == 2) { dst = cB.data(); haveB = true; }
        else { dst = cA.data(); haveA = true; }
        if (!decodeChannel(cur, lw, lh, dst, err)) return false;
      } else if (cid == -2) {
        // User mask. Use the mask rect from the record.
        const int mw = std::max(0, r.maskRight - r.maskLeft);
        const int mh = std::max(0, r.maskBottom - r.maskTop);
        maskW = mw;
        maskH = mh;
        if (mw == 0 || mh == 0) {
          cur.skip(2);
          continue;
        }
        cMask.resize(static_cast<std::size_t>(mw) * mh);
        if (!decodeChannel(cur, mw, mh, cMask.data(), err)) return false;
      } else {
        // Skip unknown channels: their per-channel block has a u16
        // compression + N bytes; but we don't know N without decoding.
        // The record carried a `len` field — use that. We need to skip
        // exactly `len` bytes from the channel-data section.
        // Find the matching length for this channel id; bail otherwise.
        std::uint32_t skipLen = 0;
        for (const auto& [c2, l2] : r.channels) {
          if (c2 == cid) { skipLen = l2; break; }
        }
        cur.skip(skipLen);
      }
    }
    (void)haveR; (void)haveG; (void)haveB;
    if (lw > 0 && lh > 0) {
      layerImages[i] = buildRGBAImage(
          lw, lh, cR.data(), cG.data(), cB.data(), haveA ? cA.data() : nullptr);
    }
    if (!cMask.empty()) {
      layerMasks[i] = std::move(cMask);
      maskWs[i] = maskW;
      maskHs[i] = maskH;
    }
  }

  // Pad layer-info section to 2 bytes.
  cur.seek(layerInfoEnd);
  if ((layerInfoLen & 1u) != 0) cur.skip(1);

  // Build the LayerTree from records + decoded images. PSD section
  // dividers bracket a group's contents in BOTTOM-TO-TOP order:
  //   <type 3 bounding-divider record>   ← bottom of group
  //   ... pixel records inside the group ...
  //   <type 1 or 2 folder-header record> ← top of group (carries
  //                                          name/blend/opacity)
  // We see them in that order while iterating bottom-to-top. Strategy:
  // type 3 starts a new pending group; type 1/2 finalizes it with the
  // header record's metadata; intermediate records are children. The
  // group is attached to the parent scope at finalize time so its slot
  // sits AT the position of the header record (matches PS panel order).
  std::vector<GroupLayer*> groupStack;
  auto attach = [&outDoc, &groupStack](std::unique_ptr<LayerBase> l) {
    if (groupStack.empty()) {
      outDoc.tree().add(std::move(l));
    } else {
      groupStack.back()->children.push_back(std::move(l));
    }
  };
  for (std::uint16_t i = 0; i < layerCount; ++i) {
    auto& r = recs[i];
    if (r.sectionDividerType == 3) {
      // Open a new group. Name + blend will be supplied by the matching
      // type 1/2 record we encounter later. Attach the group to the
      // current parent NOW so its slot is reserved; children get
      // appended directly into its `children` vector.
      auto g = std::make_unique<GroupLayer>();
      g->id = outDoc.nextLayerId();
      g->name = "Group";  // placeholder; folder-header record overrides
      GroupLayer* gPtr = g.get();
      attach(std::move(g));
      groupStack.push_back(gPtr);
    } else if (r.sectionDividerType == 1 || r.sectionDividerType == 2) {
      // Folder-header record. Carries the group's actual name, blend,
      // opacity, visibility. Pop the current group; apply props.
      if (!groupStack.empty()) {
        GroupLayer* gPtr = groupStack.back();
        groupStack.pop_back();
        if (!r.name.empty()) gPtr->name = r.name;
        gPtr->blend = r.sectionDividerBlend;
        gPtr->opacity = r.opacity / 255.f;
        gPtr->visible = (r.flags & 0x02) == 0;
        gPtr->isExpanded = (r.sectionDividerType == 1);
      }
    } else {
      auto px = std::make_unique<PixelLayer>();
      px->id = outDoc.nextLayerId();
      px->name = r.name.empty() ? std::string("Layer") : r.name;
      px->blend = r.blend;
      px->opacity = r.opacity / 255.f;
      px->visible = (r.flags & 0x02) == 0;
      px->originX = r.left;
      px->originY = r.top;
      px->image = std::move(layerImages[i]);
      if (!layerMasks[i].empty()) {
        auto m = std::make_unique<LayerMask>(maskWs[i], maskHs[i]);
        // Set mask pixels: mask is a single-channel byte buffer; map to
        // R channel of the LayerMask's TuxImage. .g/.b ignored by the
        // mask sampler, but we set them to .r for visual debug
        // consistency.
        for (int y = 0; y < maskHs[i]; ++y) {
          for (int x = 0; x < maskWs[i]; ++x) {
            const float v = layerMasks[i][static_cast<std::size_t>(y) *
                                            maskWs[i] + x] / 255.f;
            m->image.setPixel(x, y, Rgba32F{v, v, v, 1.f});
          }
        }
        m->enabled = (r.maskFlags & 0x02) == 0;  // bit 1 = disabled
        px->mask = std::move(m);
        // PSD masks have their own origin (maskTop/Left) which can
        // differ from the layer's; for M12 we don't preserve that
        // separately — caller can edit later.
      }
      attach(std::move(px));
    }
  }
  // Doc dimension fallback.
  (void)docW;
  (void)docH;
  return cur.ok();
}

// Top-level dispatcher. Reads header, skips Color Mode Data + Image
// Resources, then layer-and-mask info, then ignores the merged image
// data. Returns the populated Document.
std::optional<std::unique_ptr<Document>> loadInternal(
    std::span<const std::uint8_t> bytes, std::string* err) {
  Cursor cur(bytes);
  Header h;
  if (!readHeader(cur, h, err)) return std::nullopt;

  // Color Mode Data section: u32 length + bytes. Always 0 for RGB mode;
  // we just skip whatever's there.
  const std::uint32_t cmdLen = cur.u32();
  cur.skip(cmdLen);

  // Image Resources section: u32 length + bytes (resource records).
  const std::uint32_t irLen = cur.u32();
  cur.skip(irLen);

  if (!cur.ok()) {
    setErr(err, "Truncated PSD before layer-and-mask info");
    return std::nullopt;
  }

  // Layer and Mask Info section: u32 length + content.
  const std::uint32_t lmiLen = cur.u32();
  if (!cur.ok()) {
    setErr(err, "Truncated layer-and-mask info length");
    return std::nullopt;
  }
  const std::size_t lmiStart = cur.pos();
  const std::size_t lmiEnd = lmiStart + lmiLen;

  auto doc = std::make_unique<Document>(static_cast<int>(h.width),
                                         static_cast<int>(h.height));

  if (lmiLen > 0) {
    if (!readLayerInfo(cur, *doc, static_cast<int>(h.width),
                        static_cast<int>(h.height), err)) {
      return std::nullopt;
    }
  }
  cur.seek(lmiEnd);

  // The merged image data section follows; for M12 we don't synthesise a
  // composite layer from it — the per-layer images already give the user
  // an editable tree. If the file had no layer info (lmiLen == 0), the
  // user gets an empty document; a future step can fall back to reading
  // the merged image as a single PixelLayer.

  // Pick a sensible active layer.
  if (!doc->tree().empty()) {
    doc->setActiveLayerId(doc->tree().at(doc->tree().size() - 1)->id);
  }

  return doc;
}

}  // namespace

std::optional<std::unique_ptr<Document>> loadPsdBytes(
    std::span<const std::uint8_t> bytes, std::string* err) {
  return loadInternal(bytes, err);
}

std::optional<std::unique_ptr<Document>> loadPsd(const std::string& path,
                                                  std::string* err) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    setErr(err, "Cannot open file: " + path);
    return std::nullopt;
  }
  in.seekg(0, std::ios::end);
  const std::streamsize size = in.tellg();
  in.seekg(0, std::ios::beg);
  if (size <= 0) {
    setErr(err, "Empty / unreadable file: " + path);
    return std::nullopt;
  }
  std::vector<std::uint8_t> buf(static_cast<std::size_t>(size));
  in.read(reinterpret_cast<char*>(buf.data()), size);
  if (!in) {
    setErr(err, "Read error: " + path);
    return std::nullopt;
  }
  return loadPsdBytes(std::span<const std::uint8_t>(buf.data(), buf.size()),
                       err);
}

}  // namespace tuxels
