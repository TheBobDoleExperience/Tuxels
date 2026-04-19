#pragma once

#include <QString>
#include <optional>

#include "core/TuxImage.h"

namespace tuxels {

// Load an 8-bit PNG (RGB/RGBA) as a TuxImage of 32-bit float per channel.
// Treats the file as sRGB and does NOT linearize for M0 (sRGB passthrough —
// see docs/ARCHITECTURE.md §7). Returns std::nullopt on failure; when `err`
// is non-null it receives a short human-readable reason.
std::optional<TuxImage> loadPng(const QString& path, QString* err = nullptr);

// Save a TuxImage as an 8-bit sRGB PNG. Float channels are clamped to [0,1]
// and quantized with round-to-nearest. Returns true on success; when `err`
// is non-null on failure it receives a short human-readable reason.
bool savePng(const QString& path, const TuxImage& image, QString* err = nullptr);

}  // namespace tuxels
