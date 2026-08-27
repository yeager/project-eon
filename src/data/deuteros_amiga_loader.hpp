#pragma once

#include "data/amiga_adf.hpp"

#include <array>
#include <cstdint>

namespace eon {

struct AmigaLoadStage {
    std::uint32_t disk_offset = 0;
    std::uint32_t length = 0;
    std::uint32_t destination = 0;
    std::uint32_t entry_address = 0;
};

struct DeuterosAmigaLoadPlan {
    AmigaLoadStage bootstrap_loader;
    AmigaLoadStage main_stage;
    std::array<std::uint32_t, 5> resource_disk_offsets{};
};

// Decode load constants from the genuine 68000 instructions. Every expected
// opcode is checked before its immediate value is accepted.
[[nodiscard]] DeuterosAmigaLoadPlan parse_deuteros_amiga_load_plan(const AmigaAdf& disk);

} // namespace eon
