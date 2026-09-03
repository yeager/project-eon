#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace eon {

// Decoded form of the nibble-coded bitmap resources used by Millennium's
// DOS release. Pixels are row-major palette indices; no replacement artwork
// or inferred pixels are introduced by this decoder.
struct MillenniumDosBitmap {
    std::uint8_t flags = 0;
    std::uint8_t max_palette_index = 0;
    std::uint8_t codec = 0;
    std::uint16_t deduction = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint16_t encoded_span = 0;
    std::vector<std::uint8_t> pixels;
};

// An opaque byte range whose meaning has not been recovered.  Its coordinates
// are relative to the bitmap record supplied to the decoder.  Recording this
// identity rather than a duplicate buffer keeps the original medium as the
// sole source of its bytes.
struct MillenniumDosOpaqueRange {
    std::uint32_t source_offset = 0;
    std::uint32_t length = 0;
    std::string sha256;
};

// The VGA form of a bitmap record appends its 256-entry RGB6 DAC table, a
// separately retained 36-byte table, then a logical-index to DAC-index table
// after the compressed pixels.
// RGB6 components are retained exactly as stored (0..63); conversion to RGB8
// is only a presentation adapter.
struct MillenniumDosPalette {
    std::array<std::array<std::uint8_t, 3>, 256> dac_rgb6{};
    // Located immediately after RGB6 data but not the TITLES.EXE mode-1 XLAT
    // table. Its purpose remains deliberately neutral, so retain only its
    // original source range and identity rather than a copied byte buffer.
    MillenniumDosOpaqueRange auxiliary_translation;
    std::vector<std::uint8_t> logical_to_dac;
};

[[nodiscard]] MillenniumDosBitmap decode_millennium_dos_bitmap(
    std::span<const std::uint8_t> bytes);

[[nodiscard]] MillenniumDosPalette decode_millennium_dos_palette(
    std::span<const std::uint8_t> bytes, const MillenniumDosBitmap& bitmap);

// Produces SDL-ready RGBA pixels from authentic bitmap indices and the
// authentic RGB6 DAC table. VGA itself receives the unscaled RGB6 values;
// each component is expanded by bit replication: (value << 2) | (value >> 4).
[[nodiscard]] std::vector<std::uint8_t> colorize_millennium_dos_bitmap(
    const MillenniumDosBitmap& bitmap, const MillenniumDosPalette& palette);

} // namespace eon
