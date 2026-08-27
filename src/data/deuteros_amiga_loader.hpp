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

// Facts recovered from the first straight-line and loop setup portion of the
// raw main stage.  These are addresses and literal values only: they do not
// assign gameplay meaning to the state cells or service calls.
struct DeuterosAmigaMainStageEntry {
    std::uint32_t entry_address = 0;
    std::uint32_t incoming_controller_cell = 0;
    std::uint32_t incoming_mode_cell = 0;
    std::uint32_t stack_address = 0;
    std::uint32_t memory_ceiling = 0;
    std::array<std::uint32_t, 2> initialization_calls{};
    std::uint32_t loop_address = 0;
    std::uint32_t loop_first_service_address = 0;
    std::uint32_t loop_scheduler_address = 0;
    std::uint32_t first_state_word_address = 0;
    std::uint32_t second_state_word_address = 0;
    std::uint32_t scheduler_enable_word_address = 0;
    std::uint16_t scheduler_enable_word_value = 0;
    std::uint32_t first_input_address = 0;
    std::uint8_t first_input_bit = 0;
    std::uint32_t second_input_address = 0;
    std::uint8_t second_input_bit = 0;
};

struct DeuterosAmigaLoadPlan {
    AmigaLoadStage bootstrap_loader;
    AmigaLoadStage main_stage;
    DeuterosAmigaMainStageEntry main_stage_entry;
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
