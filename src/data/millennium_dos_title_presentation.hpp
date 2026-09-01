#pragma once

#include "data/millennium_dos_bitmap.hpp"
#include "data/millennium_dos_title_transition.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace eon {

// The complete, renderer-ready *data* side of the recovered English DOS
// title.  The base frame is a real P00 decode; P01..P25 remain individually
// indexed patches in the original static order.  This deliberately does not
// compose patches, select a display-driver mode, assign cadence, or connect
// a host input to TITLES.EXE.
struct MillenniumDosTitlePresentationAssets {
    std::string title_library_sha256;

    std::string base_resource_name;
    std::uint32_t base_resource_offset = 0;
    std::uint32_t base_resource_size = 0;
    std::string base_resource_sha256;
    MillenniumDosBitmap base_bitmap;
    MillenniumDosPalette base_palette;
    // SDL-ready expansion of the original indexed P00 pixels and original
    // RGB6 palette.  This is derived in memory only; no media is unpacked or
    // changed on disk.
    std::vector<std::uint8_t> base_rgba;

    MillenniumDosTitleTransitionSequence transition;
};

// Admits only the hash-recognised English TITLE.LIB and the independently
// hash-locked TITLES.EXE/MILL.COM transition profile.  Returned values are
// presentation provenance, not an emulation or title-to-game hand-off.
[[nodiscard]] MillenniumDosTitlePresentationAssets
parse_millennium_dos_title_presentation_assets(
    const MillenniumDosLib& title_library, const MillenniumDosTitleFlow& flow);

} // namespace eon
