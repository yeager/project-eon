#pragma once

#include "data/millennium_dos_bitmap.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace eon {

// A directly decoded pair from the English DOS GX.LIB.  IMG00 supplies the
// original VGA DAC and IMG01 supplies a 320x167 indexed canvas.  The latter
// is deliberately called a canvas rather than a named game screen: the byte
// layout proves the resources and their presentation path, not UI semantics.
struct MillenniumDosGameplayScreen {
    MillenniumDosBitmap palette_resource;
    MillenniumDosBitmap canvas;
    std::array<std::array<std::uint8_t, 3>, 256> dac_rgb6{};
    std::vector<std::uint8_t> canvas_logical_to_dac;
    // The remaining original resource tables are preserved as opaque bytes.
    std::vector<std::uint8_t> palette_resource_auxiliary;
    std::vector<std::uint8_t> canvas_auxiliary;
    std::vector<std::uint8_t> rgba;
};

// Reads the two verified resources directly from GX.LIB.  It does not extract
// the archive or manufacture a replacement game screen.
[[nodiscard]] MillenniumDosGameplayScreen parse_millennium_dos_gameplay_screen(
    std::span<const std::uint8_t> gx_lib);

} // namespace eon
