#pragma once

#include <cstdint>
#include <span>
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

[[nodiscard]] MillenniumDosBitmap decode_millennium_dos_bitmap(
    std::span<const std::uint8_t> bytes);

} // namespace eon
