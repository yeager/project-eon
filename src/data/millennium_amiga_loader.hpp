#pragma once

#include "data/amiga_adf.hpp"

#include <cstdint>

namespace eon {

// The supplied Millennium 2.2 Amiga images carry their game data in raw disk
// ranges.  This is a description of the requests made by the on-disk loader,
// not an extracted or installed representation of the game.
struct MillenniumAmigaLoadStage {
    std::uint32_t disk_offset = 0;
    std::uint32_t length = 0;
    std::uint32_t destination = 0;
};

struct MillenniumAmigaLoadPlan {
    MillenniumAmigaLoadStage bootstrap_loader;
    MillenniumAmigaLoadStage first_stage;
    MillenniumAmigaLoadStage resident_stage;
    std::uint32_t resident_entry = 0;
    std::uint32_t loader_magic = 0;
};

// Recovers the explicit raw-read requests from the first-stage 68000 loader.
// It validates the instruction sequence and every resulting disk range.  It
// intentionally does not decompress, write, or otherwise unpack game media.
[[nodiscard]] MillenniumAmigaLoadPlan parse_millennium_amiga_load_plan(
    const AmigaAdf& disk);

} // namespace eon
