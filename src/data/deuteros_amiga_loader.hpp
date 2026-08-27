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

// One of the bootstrap routines selected through the six-entry table at
// $12a36.  These routines only provide read constants; unlike profile zero,
// a later profile is not assumed to begin with an absolute JMP.
struct DeuterosAmigaBootstrapProfile {
    std::uint32_t disk_offset = 0;
    std::uint32_t length = 0;
    std::uint32_t destination = 0;
};

struct DeuterosAmigaLoadPlan {
    AmigaLoadStage bootstrap_loader;
    AmigaLoadStage main_stage;
    std::array<std::uint32_t, 5> resource_disk_offsets{};
    // The title's accepted-input path writes profile one to $12ffc before it
    // returns to this bootstrap. Retain the real load constants so callers
    // can hand off without guessing an unpacked game executable.
    DeuterosAmigaBootstrapProfile title_handoff_profile;
    // Profile one is itself a raw, relocatable 68000 stage.  Its first word
    // is an absolute JMP, so this supplies the real entry address without
    // treating the disk bytes as an unpacked host-side executable.
    AmigaLoadStage title_stage;
};

// Decode load constants from the genuine 68000 instructions. Every expected
// opcode is checked before its immediate value is accepted.
[[nodiscard]] DeuterosAmigaLoadPlan parse_deuteros_amiga_load_plan(const AmigaAdf& disk);

} // namespace eon
