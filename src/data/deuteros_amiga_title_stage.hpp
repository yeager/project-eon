#pragma once

#include "data/deuteros_amiga_loader.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string>
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
    // The first graphics-library call is followed by a local polling loop.
    // These are original operand cells only; no vector, display, or concurrent
    // update is modelled.
    std::uint32_t transition_first_phase_source_address = 0;
    std::uint32_t transition_first_phase_first_work_address = 0;
    std::uint32_t transition_first_phase_second_work_address = 0;
    std::uint32_t transition_first_phase_work_pointer_address = 0;
    std::uint32_t transition_poll_loop_address = 0;
    // The transition's second phase supplies three original work addresses to
    // the same graphics library after the polling loop observes a difference.
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

// The common $1bf36-zero response route following the timer gate. Its local
// $1f238 helper is separately recovered, but its pending word/byte region are
// runtime inputs, so low-byte responses remain explicit. The non-zero $1bf36
// route remains outside this intentionally narrow model.
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

// The hash-locked local response tail after the title transition returns. The
// unresolved helper's low-byte responses are supplied by the caller; no helper
// or hardware call is made. The control word starts at zero for this local
// trace and only its low byte is changed by the original ADDQ/SUBQ paths.
struct DeuterosAmigaTitlePostTransitionResponseLoop {
    std::uint32_t entry_address = 0;
    std::uint32_t feedback_tail_address = 0;
    std::uint32_t control_word_address = 0;
    std::uint16_t initial_control_word = 0;
    std::uint16_t final_control_word = 0;
    std::uint32_t helper_address = 0;
    std::uint8_t return_response = 0;
    std::uint8_t loop_response = 0;
    std::uint8_t increment_response = 0;
    std::uint8_t decrement_response = 0;
    std::vector<std::uint8_t> control_low_byte_writes;
    std::uint32_t helper_loop_address = 0;
    std::uint32_t return_address = 0;
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

// The title-entry instructions before the first Exec vector make exactly two
// caller-proven writes for the live profile-one route.  Retain them as a
// sparse, in-memory result instead of allocating an imagined Amiga address
// space or changing the source ADF.  `width_bytes` is deliberately explicit:
// the mode store is a word and the normal-mode store is a byte.
struct DeuterosAmigaTitleEntryWrite {
    std::uint32_t address = 0;
    std::uint8_t width_bytes = 0;
    std::uint16_t value = 0;
};

struct DeuterosAmigaTitleEntryPrefixState {
    std::uint16_t incoming_profile = 0;
    std::array<DeuterosAmigaTitleEntryWrite, 2> writes{};
    std::uint32_t stop_before_exec_address = 0;
};

// The first instruction after the profile-one entry stores a literal stack
// address in A7.  It is still wholly local: the very next instruction reads
// the unknown Exec base from address $4.  Keep this one-register result
// separate from the sparse title-RAM writes so callers cannot mistake it for
// an emulated Amiga stack or an Exec transition.
struct DeuterosAmigaTitleExecPrelude {
    std::uint16_t incoming_profile = 0;
    std::uint32_t entry_address = 0;
    std::uint32_t stack_pointer_value = 0;
    std::uint32_t stop_before_exec_base_read_address = 0;
};

// Materializes only the two direct stores described above.  This is the last
// wholly local execution result at the title handoff; it never supplies the
// bootstrap A1/controller value, calls Exec, or models an Amiga OS state.
[[nodiscard]] DeuterosAmigaTitleEntryPrefixState
materialize_deuteros_amiga_title_entry_prefix_state(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    std::uint16_t incoming_profile);

// Executes exactly `MOVEA.L #$40b62,A7` after the observed profile-one title
// prefix.  It stops before `MOVEA.L $4.W,A6`; no Exec base, vector, stack
// memory, graphics library, or custom hardware is supplied or touched.
[[nodiscard]] DeuterosAmigaTitleExecPrelude
execute_deuteros_amiga_title_exec_prelude(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    std::uint16_t incoming_profile);

// The title-entry CMP.B arm for observed low byte five. It records literal
// instruction operands only and stops before the first Exec vector.
struct DeuterosAmigaTitleEntryModeFivePrefix {
    std::uint16_t incoming_profile = 0;
    std::uint32_t controller_transfer_address = 0;
    std::uint32_t mode_word_address = 0;
    std::uint16_t mode_word_value = 0;
    std::uint32_t low_byte_destination_address = 0;
    std::uint8_t low_byte_value = 0;
    std::uint32_t literal_word_destination_address = 0;
    std::uint16_t literal_word_value = 0;
    std::uint32_t stop_before_exec_address = 0;
};

// A direct, wholly local callee in the static continuation after the two
// title-entry Exec vectors.  Its call still depends on those earlier vectors
// and intervening calls returning, so this is provenance for an instruction
// sequence, not an executable startup result.  The callee itself merely
// loads a literal longword into D0, stores it in an in-stage cell, and RTSes.
struct DeuterosAmigaTitlePostExecPointerSeedProfile {
    std::uint32_t call_site_address = 0;
    std::uint32_t caller_d1_literal = 0;
    std::uint32_t callee_address = 0;
    std::uint32_t literal_value = 0;
    std::uint32_t destination_address = 0;
    std::uint32_t return_address = 0;
    std::string call_site_sha256;
    std::string callee_sha256;
};

// The immediately following caller-connected subroutine is a fixed sequence
// of four direct local calls and RTS.  Its callees are deliberately not
// followed here: their return values and any external ABI they reach are not
// established by the supplied media.  This is an address/byte provenance
// profile, not a title-stage execution model.
struct DeuterosAmigaTitlePostExecServiceBatchProfile {
    std::uint32_t call_site_address = 0;
    std::uint32_t callee_address = 0;
    std::array<std::uint32_t, 4> direct_callee_addresses{};
    std::uint32_t return_address = 0;
    std::string call_site_sha256;
    std::string callee_sha256;
};

[[nodiscard]] DeuterosAmigaTitlePostExecServiceBatchProfile
parse_deuteros_amiga_title_post_exec_service_batch_profile(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

// The fourth and final direct edge in the post-Exec service batch targets a
// complete two-byte local RTS. It closes only this byte-exact call/return
// edge: earlier calls and the enclosing return still depend on unresolved
// original execution and ABI outcomes.
struct DeuterosAmigaTitlePostExecFourthServiceProfile {
    std::uint32_t caller_address = 0;
    std::uint32_t callee_address = 0;
    std::uint32_t caller_return_address = 0;
    std::uint32_t batch_return_address = 0;
    std::string caller_sha256;
    std::string callee_sha256;
};

[[nodiscard]] DeuterosAmigaTitlePostExecFourthServiceProfile
parse_deuteros_amiga_title_post_exec_fourth_service_profile(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

// The first direct callee in the post-Exec service batch is complete local
// setup for a single graphics-library vector.  Its vector call remains an
// ABI boundary; this records only the literal register/address operands and
// following RTS, never a graphics effect or a returned result.
struct DeuterosAmigaTitlePostExecGraphicsVectorProfile {
    std::uint32_t caller_address = 0;
    std::uint32_t entry_address = 0;
    std::uint32_t a1_literal = 0;
    std::uint32_t a0_literal = 0;
    std::uint32_t d0_literal = 0;
    std::uint32_t graphics_library_base_address = 0;
    std::int16_t graphics_library_vector = 0;
    std::uint32_t return_address = 0;
    std::string caller_sha256;
    std::string routine_sha256;
};

[[nodiscard]] DeuterosAmigaTitlePostExecGraphicsVectorProfile
parse_deuteros_amiga_title_post_exec_graphics_vector_profile(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

// The second direct callee in the post-Exec service batch is a wholly local,
// straight-line state-initialization routine.  Its caller is reached only if
// the preceding graphics vector and all earlier original calls return.  This
// profile therefore records the original instruction operands and RTS only:
// it neither writes host-side state nor implies that the title path reached
// this code.
struct DeuterosAmigaTitlePostExecStateInitProfile {
    std::uint32_t caller_address = 0;
    std::uint32_t entry_address = 0;
    std::uint32_t cleared_word_address = 0;
    std::uint16_t cleared_word_value = 0;
    std::uint32_t initial_word_address = 0;
    std::uint16_t initial_word_value = 0;
    std::uint32_t initial_long_address = 0;
    std::uint32_t initial_long_value = 0;
    std::uint32_t copied_word_source_address = 0;
    std::uint32_t copied_word_destination_address = 0;
    std::uint32_t return_address = 0;
    std::string caller_sha256;
    std::string routine_sha256;
};

[[nodiscard]] DeuterosAmigaTitlePostExecStateInitProfile
parse_deuteros_amiga_title_post_exec_state_init_profile(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

// The third direct target in the post-Exec service batch first calls a
// complete local graphics-library setup routine.  The routine's three vector
// calls remain explicit ABI boundaries.  If that routine returns, the caller
// loads a literal A6 value and tail-jumps within the title stage.  This profile
// is byte provenance only: it does not call vectors, write their operands, or
// claim that the tail target was reached at runtime.
struct DeuterosAmigaTitlePostExecThirdServiceProfile {
    std::uint32_t caller_address = 0;
    std::uint32_t dispatch_entry_address = 0;
    std::uint32_t graphics_service_address = 0;
    std::uint32_t graphics_library_base_address = 0;
    std::array<std::int16_t, 3> graphics_library_vectors{};
    std::uint32_t status_byte_address = 0;
    std::uint32_t destination_pointer_literal = 0;
    std::uint32_t destination_pointer_cell_address = 0;
    std::uint32_t descriptor_address = 0;
    std::array<std::uint16_t, 3> descriptor_offsets{};
    std::array<std::uint16_t, 3> descriptor_values{};
    std::uint32_t service_return_address = 0;
    std::uint32_t dispatcher_a6_literal = 0;
    std::uint32_t dispatcher_tail_jump_address = 0;
    std::string caller_sha256;
    std::string dispatch_sha256;
    std::string service_sha256;
};

[[nodiscard]] DeuterosAmigaTitlePostExecThirdServiceProfile
parse_deuteros_amiga_title_post_exec_third_service_profile(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

// `$1f37a` tail-jumps here only after its local graphics service returns.
// This complete local dispatch preserves the four BSR operands and its final
// RTS, but does not follow their targets: those routes contain graphics ABI
// calls whose returns and effects are not present in the supplied media.
struct DeuterosAmigaTitlePostExecTailDispatchProfile {
    std::uint32_t caller_address = 0;
    std::uint32_t entry_address = 0;
    std::array<std::uint32_t, 4> local_call_addresses{};
    std::uint32_t return_address = 0;
    std::string routine_sha256;
};

[[nodiscard]] DeuterosAmigaTitlePostExecTailDispatchProfile
parse_deuteros_amiga_title_post_exec_tail_dispatch_profile(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

// The first BSR in the recovered post-Exec tail dispatch enters this complete
// local setup routine.  It reaches a graphics.library vector after loading
// literal A0/A1 operands and externally initialized A2/A6 pointer cells.
// The vector's return and effect remain explicit ABI boundaries: this profile
// records no rendered result and does not claim that the caller continues.
struct DeuterosAmigaTitlePostExecTailFirstCalleeProfile {
    std::uint32_t caller_address = 0;
    std::uint32_t caller_continuation_address = 0;
    std::uint32_t entry_address = 0;
    std::uint32_t a0_literal = 0;
    std::uint32_t a1_literal = 0;
    std::uint32_t a2_pointer_cell_address = 0;
    std::uint32_t graphics_library_base_address = 0;
    std::int16_t graphics_library_vector = 0;
    std::uint32_t vector_return_address = 0;
    std::uint32_t routine_return_address = 0;
    std::string caller_sha256;
    std::string routine_sha256;
};

[[nodiscard]] DeuterosAmigaTitlePostExecTailFirstCalleeProfile
parse_deuteros_amiga_title_post_exec_tail_first_callee_profile(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

// The second BSR in the recovered tail dispatch re-enters this wholly local
// routine after the first graphics vector would have returned.  The routine
// contains two mirrored, bounded word-selection blocks and one further
// graphics-library vector.  Its input cells and vector result remain outside
// the supplied media, so this is raw control-flow and operand provenance only.
struct DeuterosAmigaTitlePostExecTailSecondCalleeProfile {
    std::uint32_t caller_address = 0;
    std::uint32_t caller_continuation_address = 0;
    std::uint32_t entry_address = 0;
    std::array<std::uint32_t, 6> selection_cells{};
    std::uint32_t a0_literal = 0;
    std::uint32_t a1_literal = 0;
    std::uint16_t d0_addend = 0;
    std::uint16_t d1_adjustment_opcode = 0;
    std::uint16_t d1_shift_opcode = 0;
    std::uint32_t graphics_library_base_address = 0;
    std::int16_t graphics_library_vector = 0;
    std::uint32_t vector_return_address = 0;
    std::uint32_t routine_return_address = 0;
    std::string caller_sha256;
    std::string routine_sha256;
};

[[nodiscard]] DeuterosAmigaTitlePostExecTailSecondCalleeProfile
parse_deuteros_amiga_title_post_exec_tail_second_callee_profile(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

// The third BSR in the same tail dispatch is a distinct caller-connected
// re-entry of the preceding local routine.  It is kept separate so the
// original control-flow edge is not collapsed into the earlier call site.
// Its vector boundary, inputs, and effects remain outside the supplied media.
struct DeuterosAmigaTitlePostExecTailThirdCalleeProfile {
    std::uint32_t caller_address = 0;
    std::uint32_t caller_continuation_address = 0;
    std::uint32_t entry_address = 0;
    std::uint32_t routine_return_address = 0;
    std::string caller_sha256;
    std::string routine_sha256;
};

[[nodiscard]] DeuterosAmigaTitlePostExecTailThirdCalleeProfile
parse_deuteros_amiga_title_post_exec_tail_third_callee_profile(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

// The fourth and last BSR in the same tail dispatch is another distinct edge
// into a byte-identical local graphics-vector wrapper.  Keep the different
// entry address and caller continuation visible: identical instructions are
// not evidence that the original control-flow edges may be merged.  Pointer
// cells, vector return, and all graphics effects remain ABI boundaries.
struct DeuterosAmigaTitlePostExecTailFourthCalleeProfile {
    std::uint32_t caller_address = 0;
    std::uint32_t caller_continuation_address = 0;
    std::uint32_t entry_address = 0;
    std::uint32_t a0_literal = 0;
    std::uint32_t a1_literal = 0;
    std::uint32_t a2_pointer_cell_address = 0;
    std::uint32_t graphics_library_base_address = 0;
    std::int16_t graphics_library_vector = 0;
    std::uint32_t vector_return_address = 0;
    std::uint32_t routine_return_address = 0;
    std::string caller_sha256;
    std::string routine_sha256;
};

[[nodiscard]] DeuterosAmigaTitlePostExecTailFourthCalleeProfile
parse_deuteros_amiga_title_post_exec_tail_fourth_callee_profile(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

// When all four BSRs in the static post-Exec tail have returned, the original
// batch itself returns to this exact continuation.  It transfers two
// longwords from one literal table address, then calls a wholly local wrapper
// whose final Exec vector is deliberately retained as an ABI boundary.  This
// is byte provenance only: neither the table contents nor the vector result
// is supplied or interpreted by Project Eon.
struct DeuterosAmigaTitlePostExecTailReturnProfile {
    std::uint32_t continuation_address = 0;
    std::uint32_t source_table_address = 0;
    std::array<std::uint32_t, 2> destination_addresses{};
    std::uint32_t local_service_call_address = 0;
    std::uint32_t local_service_address = 0;
    std::uint32_t service_a1_literal = 0;
    std::array<std::uint16_t, 4> service_a1_offsets{};
    std::array<std::uint32_t, 2> service_long_literals{};
    std::uint32_t exec_base_address = 0;
    std::int16_t exec_vector = 0;
    std::uint32_t vector_return_address = 0;
    std::uint32_t routine_return_address = 0;
    std::string continuation_sha256;
    std::string routine_sha256;
};

[[nodiscard]] DeuterosAmigaTitlePostExecTailReturnProfile
parse_deuteros_amiga_title_post_exec_tail_return_profile(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

// This is the original caller continuation beginning immediately after the
// post-Exec tail wrapper's RTS.  It is reachable only if the wrapper's final
// Exec vector returns.  The profile records direct/indirect call operands and
// local branch facts through the first later in-stage flag gate; it neither
// enters any called code nor supplies results for any earlier ABI boundary.
struct DeuterosAmigaTitlePostExecTailReturnContinuationProfile {
    std::uint32_t continuation_address = 0;
    std::uint32_t preceding_local_service_address = 0;
    std::uint32_t preceding_exec_vector_return_address = 0;
    std::uint32_t preceding_local_return_address = 0;
    std::array<std::uint32_t, 13> direct_call_addresses{};
    std::uint32_t indirect_call_pointer_literal = 0;
    std::uint32_t indirect_call_address = 0;
    std::uint32_t mode_cell_address = 0;
    std::uint16_t mode_value = 0;
    std::array<std::uint32_t, 2> mode_call_targets{};
    std::uint32_t timer_counter_address = 0;
    std::uint32_t timer_limit = 0;
    std::uint32_t timer_inhibit_cell_address = 0;
    std::uint16_t timer_inhibit_value = 0;
    std::uint32_t timer_local_call_target = 0;
    std::uint32_t terminal_flag_cell_address = 0;
    std::uint32_t stop_before_address = 0;
    std::string sha256;
};

[[nodiscard]] DeuterosAmigaTitlePostExecTailReturnContinuationProfile
parse_deuteros_amiga_title_post_exec_tail_return_continuation_profile(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

// `$40504` is a caller-connected absolute call in the static post-Exec
// continuation.  Its local target selects one of two literal pointers based
// on raw flag bytes, then branches into an already profiled graphics wrapper.
// This is strictly byte provenance: Project Eon neither reads the flags,
// takes a branch, follows the wrapper ABI, nor attributes a presentation
// effect to either pointer.
struct DeuterosAmigaTitlePostExecPointerRouteProfile {
    std::uint32_t caller_address = 0;
    std::uint32_t caller_continuation_address = 0;
    std::uint32_t entry_address = 0;
    std::uint32_t entry_local_call_target = 0;
    std::uint32_t entry_clear_flag_address = 0;
    std::uint32_t entry_return_address = 0;
    std::uint32_t selected_flag_address = 0;
    std::uint32_t selected_pointer_cell_address = 0;
    std::uint32_t selected_pointer_literal = 0;
    std::uint8_t selected_flag_value = 0;
    std::uint32_t selected_branch_target = 0;
    std::uint32_t alternate_entry_address = 0;
    std::uint32_t alternate_flag_address = 0;
    std::uint32_t alternate_pointer_cell_address = 0;
    std::uint32_t alternate_pointer_literal = 0;
    std::uint32_t alternate_branch_target = 0;
    std::uint32_t alternate_return_address = 0;
    std::string caller_sha256;
    std::string routine_sha256;
};

[[nodiscard]] DeuterosAmigaTitlePostExecPointerRouteProfile
parse_deuteros_amiga_title_post_exec_pointer_route_profile(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

// `$40616` begins with the two opcode bytes retained at the end of the
// preceding return-continuation evidence span.  This overlapping profile
// binds the complete following conditional block through its local branch
// back into the title loop.  It records operands only: calls, the absolute
// jump, and the custom-chip word destination are never invoked, followed, or
// written by Project Eon.
struct DeuterosAmigaTitlePostExecTailFlagGateProfile {
    std::uint32_t entry_address = 0;
    std::uint32_t preceding_profile_stop_address = 0;
    std::array<std::uint32_t, 2> source_word_addresses{};
    std::uint16_t first_compare_value = 0;
    std::uint32_t first_branch_target = 0;
    std::uint32_t second_branch_target = 0;
    std::uint32_t absolute_jump_target = 0;
    std::array<std::uint32_t, 3> direct_call_targets{};
    std::uint16_t response_compare_value = 0;
    std::uint32_t mode_cell_address = 0;
    std::uint16_t mode_compare_value = 0;
    std::array<std::uint16_t, 2> word_literals{};
    std::uint32_t custom_chip_base_address = 0;
    std::uint16_t custom_chip_word_offset = 0;
    std::uint32_t local_loop_address = 0;
    std::uint32_t exit_branch_target = 0;
    std::uint32_t stop_after_address = 0;
    std::string sha256;
};

[[nodiscard]] DeuterosAmigaTitlePostExecTailFlagGateProfile
parse_deuteros_amiga_title_post_exec_tail_flag_gate_profile(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

// The first direct call in the post-Exec tail flag gate reaches a wholly
// local, return-bounded polling routine.  Its two loops read original runtime
// cells, whose values remain unavailable to Project Eon; this profile records
// only the caller-connected instruction layout and both RTS boundaries.
struct DeuterosAmigaTitlePostExecTailFlagGateFirstCalleeProfile {
    std::uint32_t caller_address = 0;
    std::uint32_t caller_continuation_address = 0;
    std::uint32_t entry_address = 0;
    std::uint32_t tested_byte_address = 0;
    std::uint32_t zero_return_address = 0;
    std::uint32_t first_loop_word_address = 0;
    std::uint8_t first_loop_mask = 0;
    std::uint32_t first_loop_branch_address = 0;
    std::uint32_t first_loop_branch_target = 0;
    std::uint32_t second_loop_word_address = 0;
    std::uint8_t second_loop_shift_count = 0;
    std::uint32_t second_loop_branch_address = 0;
    std::uint32_t second_loop_branch_target = 0;
    std::uint32_t terminal_return_address = 0;
    std::string caller_sha256;
    std::string routine_sha256;
};

[[nodiscard]] DeuterosAmigaTitlePostExecTailFlagGateFirstCalleeProfile
parse_deuteros_amiga_title_post_exec_tail_flag_gate_first_callee_profile(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

// Both remaining direct calls in the post-Exec tail flag gate enter this
// return-bounded local routine.  It conditionally transfers one byte between
// two original runtime pointers, then performs a D1-based delay loop and
// increments the gate cell.  The original cell contents and branch result are
// unavailable, so this is provenance only:
// Project Eon neither reads nor writes either address and never executes it.
struct DeuterosAmigaTitlePostExecTailFlagGateCopyCalleeProfile {
    std::array<std::uint32_t, 2> caller_addresses{};
    std::array<std::uint32_t, 2> caller_continuation_addresses{};
    std::uint32_t entry_address = 0;
    std::uint32_t gate_word_address = 0;
    std::uint32_t zero_branch_target = 0;
    std::uint32_t source_address = 0;
    std::uint32_t destination_address = 0;
    std::uint16_t transferred_byte_count = 0;
    std::uint8_t delay_loop_counter = 0;
    std::uint32_t copy_loop_address = 0;
    std::uint32_t copy_loop_branch_address = 0;
    std::uint32_t copy_loop_branch_target = 0;
    std::uint32_t increment_address = 0;
    std::uint32_t terminal_return_address = 0;
    std::string caller_sha256;
    std::string routine_sha256;
};

[[nodiscard]] DeuterosAmigaTitlePostExecTailFlagGateCopyCalleeProfile
parse_deuteros_amiga_title_post_exec_tail_flag_gate_copy_callee_profile(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

// The first common internal setup callee opens the literal
// `graphics.library` name through an Exec vector.  Its return is an explicit
// ABI input: zero enters the original self-loop and any nonzero longword is
// stored for later graphics vectors.  The following direct callee has a
// wholly local 20-word palette copy, but its display-memory base remains an
// externally initialized pointer cell.  This profile records those exact
// facts without opening a library, allocating memory, or drawing a screen.
struct DeuterosAmigaTitleGraphicsSetupProfile {
    std::uint32_t entry_address = 0;
    std::uint32_t library_name_address = 0;
    std::string library_name;
    std::uint32_t exec_base_address = 0;
    std::int16_t exec_vector = 0;
    std::uint32_t zero_result_loop_address = 0;
    std::uint32_t nonzero_result_store_address = 0;
    std::uint32_t nonzero_result_destination_address = 0;
    std::uint32_t first_return_address = 0;
    std::uint32_t following_entry_address = 0;
    std::uint32_t palette_copy_entry_address = 0;
    std::uint32_t external_display_base_source_address = 0;
    std::array<std::uint32_t, 2> external_display_base_destinations{};
    std::uint32_t palette_source_address = 0;
    std::uint32_t palette_destination_address = 0;
    std::array<std::uint16_t, 20> palette_words{};
    std::uint32_t derived_pointer_source_address = 0;
    std::uint32_t derived_pointer_destination_address = 0;
    std::uint32_t derived_pointer_addend = 0;
    std::uint32_t following_return_address = 0;
    std::string first_callee_sha256;
    std::string following_callee_sha256;
    std::string palette_sha256;
};

// The next direct caller-connected callee is wholly local, but its destination
// address comes from the graphics setup's externally established `$1f168`
// cell.  This profile preserves the original clear-loop operands without
// resolving that pointer, writing title-stage memory, or assigning a display
// meaning to the cleared region.
struct DeuterosAmigaTitleDisplayClearProfile {
    std::uint32_t entry_address = 0;
    std::uint32_t destination_pointer_address = 0;
    std::uint16_t initial_loop_counter = 0;
    std::uint32_t iteration_count = 0;
    std::uint32_t value = 0;
    std::uint8_t write_width_bytes = 0;
    std::uint32_t return_address = 0;
    std::string sha256;
};

// The next contiguous local routine is a bounded four-pass byte combiner. It
// rejects coordinate values outside its literal unsigned ranges, derives a
// bit from the low nibble of D0, and reads/writes through the same externally
// established `$1f168` pointer.  This is not a renderer or a display model.
struct DeuterosAmigaTitleFourPassByteCombineProfile {
    std::uint32_t entry_address = 0;
    std::uint16_t first_coordinate_minimum = 0;
    std::uint16_t first_coordinate_limit = 0;
    std::uint16_t second_coordinate_minimum = 0;
    std::uint16_t second_coordinate_limit = 0;
    std::uint16_t first_coordinate_origin = 0;
    std::uint16_t second_coordinate_origin = 0;
    std::uint16_t second_coordinate_stride = 0;
    std::uint32_t source_table_address = 0;
    std::uint16_t source_table_selector_mask = 0;
    std::uint8_t source_table_selector_shift = 0;
    std::uint32_t destination_pointer_address = 0;
    std::uint32_t pass_stride = 0;
    std::uint8_t pass_count = 0;
    std::uint32_t return_address = 0;
    std::string sha256;
};

// This caller-connected local helper waits on an original word, then returns
// a byte sourced from a fixed in-stage region while shifting twenty bytes and
// decrementing that word. The cell contents and concurrent writers remain
// runtime dependencies; this profile does not simulate the wait or queue.
struct DeuterosAmigaTitleResponseQueueProfile {
    std::uint32_t entry_address = 0;
    std::uint32_t pending_word_address = 0;
    std::uint32_t wait_branch_address = 0;
    std::uint32_t empty_branch_address = 0;
    std::uint32_t return_address = 0;
    std::uint32_t byte_region_address = 0;
    std::uint16_t shift_initial_loop_counter = 0;
    std::uint32_t shift_byte_count = 0;
    std::string sha256;
};

// Controlled original-memory inputs for one nonempty response-queue pass.
// The extra trailing byte is required because the original DBRA loop copies
// bytes 1..20 down into positions 0..19.
struct DeuterosAmigaTitleResponseQueueInput {
    std::uint16_t pending_count = 0;
    std::array<std::uint8_t, 21> bytes{};
};

// Read-only result of that one locally complete dequeue/shift operation. It
// does not model the zero-count polling loop or write original title RAM.
struct DeuterosAmigaTitleResponseQueueResult {
    std::uint8_t response_low_byte = 0;
    std::uint16_t pending_count_after = 0;
    std::array<std::uint8_t, 21> shifted_bytes{};
    std::uint32_t return_address = 0;
};

// Hash-locked facts from the original title callback registration and its
// locally visible event-one/event-two routes. These record only descriptor
// operands and caller-owned A0 offsets seen in instructions; they do not
// identify the Exec service, callback ABI, device, or event meanings.
struct DeuterosAmigaTitleCallbackRegistrationProfile {
    std::uint32_t registration_entry_address = 0;
    std::uint32_t descriptor_address = 0;
    std::uint16_t descriptor_callback_offset = 0;
    std::uint32_t callback_address = 0;
    std::uint32_t request_address = 0;
    std::uint16_t request_command_offset = 0;
    std::uint16_t request_descriptor_offset = 0;
    std::uint16_t request_command_value = 0;
    std::uint32_t exec_base_address = 0;
    std::int16_t exec_vector = 0;
    std::uint32_t registration_return_address = 0;
    std::uint16_t callback_a0_event_offset = 0;
    std::array<std::uint8_t, 3> callback_early_return_values{};
    std::uint32_t callback_event_mirror_address = 0;
    std::uint8_t callback_second_event_value = 0;
    std::uint32_t callback_second_event_gate_address = 0;
    std::uint16_t callback_second_event_a0_word_offset = 0;
    std::uint16_t callback_second_event_special_word = 0;
    std::array<std::uint16_t, 2> callback_second_event_copy_source_offsets{};
    std::array<std::uint32_t, 2> callback_second_event_copy_destinations{};
    std::uint32_t callback_second_event_service_address = 0;
    std::uint16_t callback_second_event_mask = 0;
    std::array<std::uint8_t, 2> callback_second_event_accepted_values{};
    std::uint16_t callback_second_event_transform_source_offset = 0;
    std::uint32_t callback_second_event_transform_destination_address = 0;
    std::uint8_t callback_producer_value = 0;
    std::uint16_t callback_a0_word_offset = 0;
    std::uint16_t callback_producer_selector_offset = 0;
    std::uint16_t callback_producer_selector_mask = 0;
    std::uint16_t callback_producer_second_half_adjustment = 0;
    std::uint16_t callback_pending_limit = 0;
    std::uint32_t callback_pending_word_address = 0;
    std::uint32_t callback_source_table_address = 0;
    std::uint32_t callback_source_table_byte_count = 0;
    std::string callback_source_table_sha256;
    std::uint32_t callback_destination_address = 0;
    std::string registration_sha256;
    std::string callback_sha256;
};

// Controlled inputs for the callback's one-byte producer route only. These
// are caller-provided original-frame values, not a host input binding or an
// asserted Exec callback invocation.
struct DeuterosAmigaTitleCallbackProducerInput {
    std::uint16_t caller_word_at_6 = 0;
    std::uint16_t caller_word_at_8 = 0;
    std::uint16_t pending_count = 0;
};

// A read-only trace of the accepted byte-one producer route. `queued_byte`
// comes from the hash-locked original title stage; no original queue cell is
// modified.
struct DeuterosAmigaTitleCallbackProducerResult {
    std::uint32_t mirrored_event_address = 0;
    std::uint8_t mirrored_event_value = 0;
    std::uint32_t selector_word_address = 0;
    std::uint16_t selector_word_value = 0;
    std::uint32_t source_table_address = 0;
    std::uint16_t source_table_index = 0;
    std::uint8_t queued_byte = 0;
    std::uint32_t destination_address = 0;
    std::uint16_t destination_offset = 0;
    std::uint16_t pending_count_after = 0;
};

// Explicit frame values for the callback's byte-two arm. The gate value is
// supplied because it is original mutable title-stage state, not media.
struct DeuterosAmigaTitleCallbackSecondEventInput {
    bool gate_is_zero = false;
    std::uint16_t caller_word_at_6 = 0;
    std::uint16_t caller_word_at_8 = 0;
    std::uint16_t caller_word_at_10 = 0;
    std::uint16_t caller_word_at_12 = 0;
};

enum class DeuterosAmigaTitleCallbackSecondEventStop {
    gate_return,
    ordinary_return,
    external_service_boundary,
};

// The byte-two route's local write trace. No original memory is changed; the
// service route stops before its unknown callee.
struct DeuterosAmigaTitleCallbackSecondEventResult {
    std::uint32_t mirrored_event_address = 0;
    std::uint8_t mirrored_event_value = 0;
    std::array<std::uint32_t, 2> copied_word_destinations{};
    std::array<std::uint16_t, 2> copied_word_values{};
    bool copied_words_written = false;
    std::uint32_t transformed_word_destination = 0;
    std::uint16_t transformed_word_value = 0;
    bool transformed_word_written = false;
    DeuterosAmigaTitleCallbackSecondEventStop stop =
        DeuterosAmigaTitleCallbackSecondEventStop::ordinary_return;
    std::uint32_t next_address = 0;
};

// The first known title-stage exit has a fixed, conditional in-memory byte
// copy before its already validated bootstrap-profile tail.  The two prior
// calls and the subsequent BSR remain explicit boundaries: this result is
// only the original source bytes and transfer provenance, never a write to
// the original runtime address or an assertion that either helper returned.
struct DeuterosAmigaFirstTitleExitCopy {
    std::uint32_t entry_address = 0;
    std::array<std::uint32_t, 2> preceding_helper_addresses{};
    std::uint32_t source_address = 0;
    std::uint32_t source_disk_offset = 0;
    std::uint32_t destination_address = 0;
    std::uint32_t byte_count = 0;
    std::string source_sha256;
    std::vector<std::uint8_t> copied_bytes;
    std::uint32_t stop_before_subroutine_address = 0;
};

// The first exit's bootstrap tail is reachable only when its preceding BSR
// returns.  It records instruction-level destinations, never the controller
// longword itself and never an executed JMP.
struct DeuterosAmigaFirstTitleExitReturnTail {
    std::uint32_t entry_address = 0;
    std::uint32_t preceding_subroutine_address = 0;
    std::uint32_t controller_source_address = 0;
    std::uint32_t controller_destination_address = 0;
    std::uint32_t bootstrap_profile_address = 0;
    std::uint32_t bootstrap_profile_value = 0;
    std::uint32_t jump_target_address = 0;
};

// The BSR immediately before the first exit's profile-two tail has a complete
// in-stage implementation. This is a static instruction profile only: its
// initial service call and every Exec vector call remain external ABI
// boundaries, and the two compared longwords are not assigned a role.
struct DeuterosAmigaFirstTitleExitSubroutineProfile {
    std::uint32_t entry_address = 0;
    std::uint32_t initial_d1_value = 0;
    std::uint32_t initial_d7_value = 0;
    std::uint32_t initial_d0_value = 0;
    std::uint32_t initial_service_address = 0;
    std::uint32_t first_work_address = 0;
    std::uint16_t first_work_word_offset = 0;
    std::uint16_t first_work_word_value = 0;
    std::uint16_t first_work_long_offset = 0;
    std::uint32_t first_work_long_value = 0;
    std::array<std::uint32_t, 5> exec_argument_addresses{};
    std::array<std::int16_t, 5> exec_vectors{};
    std::uint32_t compare_first_address = 0;
    std::uint32_t compare_second_address = 0;
    std::uint32_t unequal_branch_address = 0;
    std::uint32_t return_address = 0;
};

// The second known title exit reaches its bootstrap tail only after four
// original calls return. This preserves the straight-line instruction facts
// after those calls, without entering a helper, reading the controller
// longword, or executing its final jump.
struct DeuterosAmigaSecondTitleExitReturnTail {
    std::uint32_t entry_address = 0;
    std::array<std::uint32_t, 4> preceding_helper_addresses{};
    std::uint32_t controller_source_address = 0;
    std::uint32_t controller_destination_address = 0;
    std::uint32_t bootstrap_profile_address = 0;
    std::uint32_t bootstrap_profile_value = 0;
    std::uint32_t jump_target_address = 0;
};

// The third known title exit has a distinct, independently hash-locked tail.
// Its four original predecessors remain caller-supplied return boundaries;
// this type reports only the following instruction operands and final jump
// target without reading/writing original runtime memory.
struct DeuterosAmigaThirdTitleExitReturnTail {
    std::uint32_t entry_address = 0;
    std::array<std::uint32_t, 4> preceding_helper_addresses{};
    std::uint32_t controller_source_address = 0;
    std::uint32_t controller_destination_address = 0;
    std::uint32_t bootstrap_profile_address = 0;
    std::uint32_t bootstrap_profile_value = 0;
    std::uint32_t jump_target_address = 0;
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
// supplied bytes are the low bytes returned by the locally recovered `$1f238`
// helper with still-unknown runtime state; no helper or custom-chip call is
// emulated.
[[nodiscard]] DeuterosAmigaTitleZeroResponseLoop
evaluate_deuteros_amiga_title_zero_response_loop(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    std::span<const std::uint8_t> helper_response_low_bytes);

// Evaluates the local post-transition response tail only. Every supplied
// response is the low byte from the unresolved original helper; the sequence
// must end with the exact return response and cannot claim helper execution.
[[nodiscard]] DeuterosAmigaTitlePostTransitionResponseLoop
evaluate_deuteros_amiga_title_post_transition_response_loop(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    std::span<const std::uint8_t> helper_response_low_bytes);

// Validates the bootstrap D0=1 route and returns only its local title-entry
// writes. Other profiles and the controller pointer are rejected/bounded.
[[nodiscard]] DeuterosAmigaTitleEntryPrefix
execute_deuteros_amiga_title_entry_prefix(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    std::uint16_t incoming_profile);

[[nodiscard]] DeuterosAmigaTitleEntryModeFivePrefix
execute_deuteros_amiga_title_entry_mode_five_prefix(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    std::uint16_t incoming_profile);

// Parses the direct local `$403e6` callee and its `$404c2` call site in the
// static continuation after the hard Exec boundary.  It never assumes that
// the earlier vectors or calls returned and does not write title-stage memory.
[[nodiscard]] DeuterosAmigaTitlePostExecPointerSeedProfile
parse_deuteros_amiga_title_post_exec_pointer_seed_profile(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

// Parses the two caller-connected common setup callees without
// executing their Exec vector or resolving either external pointer cell.
[[nodiscard]] DeuterosAmigaTitleGraphicsSetupProfile
parse_deuteros_amiga_title_graphics_setup_profile(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

// Parses the immediate local `$1f182` clear loop that follows graphics setup.
// It does not resolve `$1f168` or perform any of the encoded writes.
[[nodiscard]] DeuterosAmigaTitleDisplayClearProfile
parse_deuteros_amiga_title_display_clear_profile(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

// Parses the contiguous local `$1f196` byte-combine routine. It records its
// gates and operands, but does not provide D0/D2/D3, resolve `$1f168`, or
// perform reads/writes through that pointer.
[[nodiscard]] DeuterosAmigaTitleFourPassByteCombineProfile
parse_deuteros_amiga_title_four_pass_byte_combine_profile(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

// Parses the direct `$1f230` wait/shift helper used by known title response
// paths. It does not supply the pending word, read the returned byte, or make
// the encoded in-stage writes.
[[nodiscard]] DeuterosAmigaTitleResponseQueueProfile
parse_deuteros_amiga_title_response_queue_profile(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

// Models one nonempty pass after the original pending-word poll. The caller
// explicitly supplies the original queue state; zero is rejected because the
// original code then spins pending an unrecovered concurrent writer.
[[nodiscard]] DeuterosAmigaTitleResponseQueueResult
evaluate_deuteros_amiga_title_response_queue_once(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    DeuterosAmigaTitleResponseQueueInput input);

[[nodiscard]] DeuterosAmigaTitleCallbackRegistrationProfile
parse_deuteros_amiga_title_callback_registration_profile(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

// Evaluates only the fully local callback byte-one producer route with
// explicit caller-frame values. It validates the registration/callback/table
// profile first, rejects out-of-route inputs, and never invokes Exec, binds a
// host input, or writes the original title-stage queue.
[[nodiscard]] DeuterosAmigaTitleCallbackProducerResult
evaluate_deuteros_amiga_title_callback_producer(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    DeuterosAmigaTitleCallbackProducerInput input);

// Evaluates only the callback's locally decoded byte-two arm using explicit
// caller-frame and gate inputs. It never invokes the $20118 service, registers
// a callback, maps host input, or writes title-stage cells.
[[nodiscard]] DeuterosAmigaTitleCallbackSecondEventResult
evaluate_deuteros_amiga_title_callback_second_event(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    DeuterosAmigaTitleCallbackSecondEventInput input);

// Validates and models only the literal byte-copy part of the first title
// exit. The model reads the supplied ADF in place; it does not invoke either
// preceding helper, enter the following subroutine, or write the original
// destination address.
[[nodiscard]] DeuterosAmigaFirstTitleExitCopy
evaluate_deuteros_amiga_first_title_exit_copy(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

// Validates the first exit's local return continuation. `subroutine_returned`
// is an explicit ABI boundary for the unresolved BSR at $37f7a. The result
// contains only raw source/destination facts; it neither reads controller
// data nor jumps to the bootstrap target.
[[nodiscard]] DeuterosAmigaFirstTitleExitReturnTail
evaluate_deuteros_amiga_first_title_exit_return_tail(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    bool subroutine_returned);

// Validates the complete original BSR target at $37f9a as a static profile.
// It neither invokes its initial service/Exec calls nor reads compared runtime
// cells; the conditional branch remains a recorded raw control-flow fact.
[[nodiscard]] DeuterosAmigaFirstTitleExitSubroutineProfile
parse_deuteros_amiga_first_title_exit_subroutine_profile(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

// `preceding_calls_returned` is an explicit boundary covering the four calls
// immediately before this tail. It must not be inferred from a title choice
// or substituted runtime state.
[[nodiscard]] DeuterosAmigaSecondTitleExitReturnTail
evaluate_deuteros_amiga_second_title_exit_return_tail(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    bool preceding_calls_returned);

// `preceding_calls_returned` covers the four calls directly before $38076;
// it is an explicit observation, never a fabricated title-stage result.
[[nodiscard]] DeuterosAmigaThirdTitleExitReturnTail
evaluate_deuteros_amiga_third_title_exit_return_tail(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    bool preceding_calls_returned);

} // namespace eon
