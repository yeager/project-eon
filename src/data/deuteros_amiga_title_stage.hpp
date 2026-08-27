#pragma once

#include "data/deuteros_amiga_loader.hpp"

#include <cstdint>

namespace eon {

// Facts recovered from the entry point of profile one.  This is intentionally
// a control-flow profile, not a claim that the original stage has been fully
// interpreted as game state.
struct DeuterosAmigaTitleStageProfile {
    std::uint32_t entry_address = 0;
    std::uint32_t incoming_mode_address = 0;
    std::uint16_t special_mode = 0;
    std::uint32_t special_mode_byte_address = 0;
    std::uint32_t special_mode_configuration_address = 0;
    std::uint16_t special_mode_configuration_value = 0;
    std::uint32_t normal_mode_byte_address = 0;
    std::uint8_t normal_mode_byte_value = 0;
    std::uint32_t main_loop_address = 0;
    std::uint32_t loop_service_address = 0;
    std::uint32_t loop_input_service_address = 0;
    std::uint32_t timer_counter_address = 0;
    std::uint32_t timer_threshold = 0;
    std::uint32_t timer_dispatch_address = 0;
    // The dispatch begins a bounded, opcode-validated palette/display
    // transition before control returns to the caller.  These are machine
    // state facts, not inferred menu/gameplay meanings.
    std::uint32_t transition_active_flag_address = 0;
    std::uint32_t transition_saved_display_word_address = 0;
    std::uint32_t transition_source_palette_address = 0;
    std::uint32_t transition_work_palette_address = 0;
    std::uint16_t transition_palette_word_count = 0;
    std::uint16_t transition_palette_mask = 0;
    std::uint32_t transition_graphics_library_base_address = 0;
    std::int16_t transition_first_library_vector = 0;
    std::int16_t transition_second_library_vector = 0;
    // The transition's second phase supplies three original work addresses to
    // the same graphics library before comparing three original display words.
    // Their roles are deliberately not inferred from these instructions.
    std::uint32_t transition_second_phase_source_address = 0;
    std::uint32_t transition_second_phase_first_work_address = 0;
    std::uint32_t transition_second_phase_second_work_address = 0;
    std::uint32_t transition_second_phase_work_pointer_address = 0;
    std::uint32_t transition_first_compare_address = 0;
    std::uint32_t transition_second_compare_address = 0;
    std::uint32_t transition_third_compare_address = 0;
    std::uint32_t transition_return_address = 0;
};

// Reads profile-one instructions directly from the original ADF and validates
// the known mode branch, recurring main-loop cadence, and the timed display
// transition dispatch.
[[nodiscard]] DeuterosAmigaTitleStageProfile parse_deuteros_amiga_title_stage(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

} // namespace eon
