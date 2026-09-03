#pragma once

#include "data/millennium_dos_bitmap.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace eon {

// Provenance for an original GX resource tail whose format has not been
// recovered. It identifies an immutable range in the admitted library
// without retaining another copy of commercial source bytes.
struct MillenniumDosGameplayOpaqueRange {
    std::uint32_t source_offset = 0;
    std::uint32_t length = 0;
    std::string sha256;
};

// A directly decoded pair from the English DOS GX.LIB.  IMG00 supplies the
// original VGA DAC and IMG01 supplies a 320x167 indexed canvas.  The latter
// is deliberately called a canvas rather than a named game screen: the byte
// layout proves the resources and their presentation path, not UI semantics.
struct MillenniumDosGameplayScreen {
    MillenniumDosBitmap palette_resource;
    MillenniumDosBitmap canvas;
    std::array<std::array<std::uint8_t, 3>, 256> dac_rgb6{};
    std::vector<std::uint8_t> canvas_logical_to_dac;
    // The remaining original resource tables stay opaque. Retain only their
    // bounded original-library ranges and digests, never duplicate bytes.
    MillenniumDosGameplayOpaqueRange palette_resource_auxiliary;
    MillenniumDosGameplayOpaqueRange canvas_auxiliary;
    std::vector<std::uint8_t> rgba;
};

// Reads the two verified resources directly from GX.LIB.  It does not extract
// the archive or manufacture a replacement game screen.
[[nodiscard]] MillenniumDosGameplayScreen parse_millennium_dos_gameplay_screen(
    std::span<const std::uint8_t> gx_lib);

} // namespace eon
