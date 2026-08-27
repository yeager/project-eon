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
    // The caller performs this equality check immediately before entering the
    // transition.  When it matches, the transition is skipped; when the
    // transition returns, the caller clears the elapsed counter again.
    std::uint32_t timer_dispatch_inhibit_address = 0;
    std::uint16_t timer_dispatch_inhibit_value = 0;
    std::uint32_t timer_counter_reset_address = 0;
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
    // The code immediately following the timed display routine forms another
    // verified state loop. It clears this word, passes it through original
    // helper calls, then compares a returned word with these literal values
    // before writing the word again. The original's higher-level labels for
    // this state and its values have not been recovered.
    std::uint32_t post_transition_control_address = 0;
    std::uint16_t post_transition_control_reset_value = 0;
    std::uint32_t post_transition_first_helper_address = 0;
    std::uint32_t post_transition_second_helper_address = 0;
    std::uint32_t post_transition_third_helper_address = 0;
    std::uint32_t post_transition_response_helper_address = 0;
    std::uint16_t post_transition_response_code = 0;
    std::uint16_t post_transition_first_compare_value = 0;
    std::uint16_t post_transition_second_compare_value = 0;
    std::uint16_t post_transition_third_compare_value = 0;
    std::uint32_t post_transition_return_address = 0;
    // The third helper does not return directly: it normalizes D0, updates a
    // title-stage byte, then jumps to this address. Both addresses are proven
    // to lie in the same title-stage load interval. This boundary proves that
    // the recovered direct route has not handed control to the main stage.
    std::uint32_t post_transition_selector_address = 0;
    std::uint32_t post_transition_selector_input_mask = 0;
    std::uint16_t post_transition_selector_first_divisor = 0;
    std::uint16_t post_transition_selector_second_divisor = 0;
    std::uint16_t post_transition_selector_addend = 0;
    std::uint32_t post_transition_selector_flag_address = 0;
    std::uint32_t post_transition_selector_dispatch_address = 0;
    // The selector's target begins a three-way signed dispatch on this byte.
    // Only the negative path's concrete display-service call is recorded;
    // this is control-flow evidence, not a name for the rendered data.
    std::uint32_t post_transition_dispatch_state_address = 0;
    std::uint32_t post_transition_dispatch_zero_branch_address = 0;
    std::uint32_t post_transition_dispatch_nonnegative_branch_address = 0;
    std::uint32_t post_transition_dispatch_negative_service_address = 0;
    std::uint16_t post_transition_dispatch_negative_service_d0 = 0;
    std::uint16_t post_transition_dispatch_negative_service_d1 = 0;
    std::uint16_t post_transition_dispatch_negative_suppress_value = 0;
    std::uint32_t post_transition_dispatch_negative_delay = 0;
};

// Reads profile-one instructions directly from the original ADF and validates
// the known mode branch, recurring main-loop cadence, timed display
// transition, and following raw control-state loop.
[[nodiscard]] DeuterosAmigaTitleStageProfile parse_deuteros_amiga_title_stage(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

} // namespace eon
