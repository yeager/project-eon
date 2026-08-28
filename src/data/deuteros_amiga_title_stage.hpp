#pragma once

#include "data/deuteros_amiga_loader.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

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
    // Positive enters this sibling byte-combine route; it has its own
    // clear/set split on the same second state byte below.
    std::uint32_t post_transition_dispatch_positive_branch_address = 0;
    // Zero selects one of two bounded byte-combine routines with this byte.
    // The pointer cells are machine facts, not inferred resource names.
    std::uint32_t post_transition_dispatch_zero_variant_state_address = 0;
    std::uint32_t post_transition_dispatch_zero_clear_variant_address = 0;
    std::uint32_t post_transition_dispatch_zero_set_variant_address = 0;
    std::uint32_t post_transition_dispatch_zero_pattern_table_address = 0;
    std::uint32_t post_transition_dispatch_zero_destination_pointer_address = 0;
    std::uint32_t post_transition_dispatch_zero_clear_source_pointer_address = 0;
    std::uint32_t post_transition_dispatch_zero_set_source_pointer_address = 0;
    std::uint32_t post_transition_dispatch_zero_pointer_advance_address = 0;
    std::uint16_t post_transition_dispatch_zero_row_advance = 0;
    std::uint16_t post_transition_dispatch_zero_plane_advance = 0;
    std::uint8_t post_transition_dispatch_zero_row_count = 0;
    std::uint8_t post_transition_dispatch_zero_plane_count = 0;
    std::uint32_t post_transition_dispatch_negative_service_address = 0;
    std::uint16_t post_transition_dispatch_negative_service_d0 = 0;
    std::uint16_t post_transition_dispatch_negative_service_d1 = 0;
    std::uint16_t post_transition_dispatch_negative_suppress_value = 0;
    std::uint32_t post_transition_dispatch_negative_delay = 0;
    // The non-suppressed service call additionally saves A0/A1 around the
    // call. Both paths then restore the earlier D5/D0 saves and return from
    // this title-stage routine; this records instruction-level preservation,
    // not a claim about the service's output.
    bool post_transition_dispatch_negative_service_preserves_a0_a1 = false;
    bool post_transition_dispatch_negative_restores_d5_then_d0 = false;
    std::uint32_t post_transition_dispatch_negative_return_address = 0;
    // Three independently reachable title-stage tails retain the bootstrap
    // controller in A1, select profile 2, 4, or 3 respectively, and JMP to
    // the bootstrap reset entry. The bootstrap table resolves all three to
    // profile zero, which is the existing raw main-stage load. These are
    // verified handoff facts, not labels for title choices or gameplay modes.
    std::uint32_t title_exit_first_address = 0;
    std::uint16_t title_exit_first_profile = 0;
    std::uint32_t title_exit_second_address = 0;
    std::uint16_t title_exit_second_profile = 0;
    std::uint32_t title_exit_third_address = 0;
    std::uint16_t title_exit_third_profile = 0;
    std::uint32_t title_exit_controller_address = 0;
    std::uint32_t title_exit_profile_slot_address = 0;
    std::uint32_t title_exit_profile_table_address = 0;
    std::uint16_t title_exit_resolved_profile = 0;
    std::uint32_t title_exit_main_stage_entry_address = 0;
    std::array<std::uint32_t, 6> bootstrap_profile_table_entries{};
    std::uint32_t bootstrap_profile_five_address = 0;
    std::uint32_t bootstrap_profile_five_first_call_address = 0;
    std::uint32_t bootstrap_profile_five_helper_controller_cell = 0;
    std::uint16_t bootstrap_profile_five_helper_long_offset = 0;
    std::uint32_t bootstrap_profile_five_helper_long_value = 0;
    std::uint16_t bootstrap_profile_five_helper_word_offset = 0;
    std::uint16_t bootstrap_profile_five_helper_word_value = 0;
    std::uint16_t bootstrap_profile_five_helper_byte_offset = 0;
    std::uint8_t bootstrap_profile_five_helper_byte_value = 0;
    std::uint32_t bootstrap_profile_five_helper_library_base = 0;
    std::int16_t bootstrap_profile_five_helper_library_vector = 0;
    // The common title-entry prefix through the first recurring loop. These
    // are opcode-validated startup requirements only: Project Eon does not
    // call Exec, write custom-chip registers, or assume their effects.
    std::uint32_t initialization_stack_address = 0;
    std::uint32_t initialization_exec_base_address = 0;
    std::array<std::int16_t, 2> initialization_exec_vectors{};
    std::uint32_t initialization_exec_allocation_size = 0;
    std::array<std::uint32_t, 11> initialization_internal_calls{};
    std::uint32_t initialization_copy_source_address = 0;
    std::array<std::uint32_t, 2> initialization_copy_destinations{};
    std::uint32_t initialization_custom_base_address = 0;
    std::array<std::uint16_t, 4> initialization_custom_offsets{};
    std::array<std::uint16_t, 4> initialization_custom_values{};
    std::uint32_t initialization_mode_five_call_address = 0;
    std::uint32_t initialization_normal_call_address = 0;
};

// The wholly local prefix of the timer-gated $4069a transition, ending just
// before its first graphics.library vector. This represents only the original
// in-memory writes and call ABI; it neither calls a library nor presents a
// title screen.
struct DeuterosAmigaTitleTransitionPrefix {
    std::uint32_t entry_address = 0;
    std::uint32_t active_flag_address = 0;
    std::uint8_t active_flag_value = 0;
    std::uint32_t saved_display_word_address = 0;
    std::uint16_t saved_display_word = 0;
    std::uint16_t cleared_display_word = 0;
    std::uint32_t source_palette_address = 0;
    std::uint32_t work_palette_address = 0;
    std::array<std::uint16_t, 16> work_palette_words{};
    std::uint32_t graphics_library_base_address = 0;
    std::int16_t graphics_library_vector = 0;
    std::uint32_t graphics_source_address = 0;
    std::uint32_t graphics_destination_address = 0;
    std::uint16_t graphics_word_count = 0;
};

// The complete local conditional branch immediately before $4069a. Counter
// reset is conditional on the original transition returning, so the evaluator
// reports that fact but never mutates caller state.
struct DeuterosAmigaTitleTimerGate {
    std::uint32_t entry_address = 0;
    std::uint32_t elapsed_counter_address = 0;
    std::uint32_t elapsed_threshold = 0;
    std::uint32_t inhibit_word_address = 0;
    std::uint16_t inhibit_word_value = 0;
    std::uint32_t skipped_target_address = 0;
    std::uint32_t transition_address = 0;
    bool dispatches_transition = false;
    bool counter_reset_after_transition_return = false;
};

// The common $1bf36-zero response route following the timer gate. $1f238 is
// an external helper, so its low-byte responses are supplied explicitly. The
// non-zero $1bf36 route remains outside this intentionally narrow model.
struct DeuterosAmigaTitleZeroResponseLoop {
    std::uint32_t entry_address = 0;
    std::uint32_t state_word_address = 0;
    std::uint16_t initial_state_word = 0;
    std::uint16_t final_state_word = 0;
    std::uint32_t helper_address = 0;
    std::uint8_t response_match_value = 0;
    std::uint32_t custom_address = 0;
    std::vector<std::uint16_t> custom_write_words;
    std::uint32_t return_loop_address = 0;
};

// The title entry's local profile-one prefix. A1's controller value is an
// explicit ABI boundary and is never materialized here.
struct DeuterosAmigaTitleEntryPrefix {
    std::uint16_t incoming_profile = 0;
    std::uint32_t controller_transfer_address = 0;
    std::uint32_t mode_word_address = 0;
    std::uint16_t mode_word_value = 0;
    std::uint32_t normal_mode_byte_address = 0;
    std::uint8_t normal_mode_byte_value = 0;
    std::uint32_t stop_before_exec_address = 0;
};

// Reads profile-one instructions directly from the original ADF and validates
// the known mode branch, recurring main-loop cadence, timed display
// transition, and following raw control-state loop.
[[nodiscard]] DeuterosAmigaTitleStageProfile parse_deuteros_amiga_title_stage(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

// Models the literal, hash-locked prefix only. The caller must establish the
// original $40410/$22d34 gate; this helper deliberately cannot imply that the
// title stage startup or graphics.library vector has executed.
[[nodiscard]] DeuterosAmigaTitleTransitionPrefix
execute_deuteros_amiga_title_transition_prefix(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    std::uint16_t input_display_word);

// Evaluates just the original unsigned threshold/inhibit branch. It does not
// update the original counter or call $4069a; that call has an unresolved
// graphics-library boundary.
[[nodiscard]] DeuterosAmigaTitleTimerGate
evaluate_deuteros_amiga_title_timer_gate(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    std::uint32_t elapsed_counter, std::uint16_t inhibit_word);

// Evaluates only the original clean-stage zero-state response loop. The
// supplied bytes are the low bytes returned by the external $1f238 helper;
// no helper or custom-chip call is emulated.
[[nodiscard]] DeuterosAmigaTitleZeroResponseLoop
evaluate_deuteros_amiga_title_zero_response_loop(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    std::span<const std::uint8_t> helper_response_low_bytes);

// Validates the bootstrap D0=1 route and returns only its local title-entry
// writes. Other profiles and the controller pointer are rejected/bounded.
[[nodiscard]] DeuterosAmigaTitleEntryPrefix
execute_deuteros_amiga_title_entry_prefix(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    std::uint16_t incoming_profile);

} // namespace eon
