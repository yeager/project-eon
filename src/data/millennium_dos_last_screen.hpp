#pragma once

#include "data/millennium_dos_bitmap.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace eon {

// LAST.LIB is a one-resource Millennium DOS library.  Its filename is an
// observation from the original media, not evidence of when the game selects
// it.  This parser only establishes the exact indexed picture and its native
// VGA DAC palette, both read directly from that library in memory.
struct MillenniumDosLastScreen {
    MillenniumDosBitmap bitmap;
    MillenniumDosPalette palette;
    std::vector<std::uint8_t> rgba;
};

[[nodiscard]] MillenniumDosLastScreen parse_millennium_dos_last_screen(
    std::span<const std::uint8_t> last_lib);

} // namespace eon
