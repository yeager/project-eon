#pragma once

#include "data/atari_st_prg.hpp"
#include "engine/millennium_atari_read_only_gemdos_session.hpp"
#include "engine/native_runtime_memory.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace eon {

enum class MillenniumAtariConfigConsumerState : std::uint8_t {
    status_register_boundary,
    xbios_trap_boundary,
    xbios_selector_three_boundary,
    xbios_selector_four_boundary,
    line_a_init_boundary,
    xbios_selector_21_boundary,
    xbios_selector_6_boundary,
    jsr_2b55a_boundary,
    bsr_2b59a_boundary,
    d0_indexed_write_boundary,
    a1_setup_boundary,
    d0_indexed_word_boundary,
    a0_indexed_word_boundary,
    loop_branch_boundary,
    movem_restore_boundary,
    jsr_2aa68_boundary,
    xbios_selector_38_boundary,
    jsr_2aa0c_boundary,
    gemdos_selector_61_boundary,
    jsr_2a5c2_boundary,
    fopen_failure_spin,
    gemdos_selector_63_boundary,
    gemdos_selector_62_boundary,
    fread_prefix_boundary,
    jsr_2b2be_boundary,
    game_init_source_byte_boundary,
    game_init_zero_copy_boundary,
    game_init_zero_counter_branch_boundary,
    game_init_complete,
    game_init_jsr_2b448_boundary,
    game_init_palette_transform_boundary,
    game_init_palette_xbios_selector_6_boundary,
    game_init_palette_outer_recurrence_boundary,
    game_init_palette_terminal_xbios_selector_6_boundary,
    game_init_palette_rts_boundary,
    game_init_bit6_clear_boundary,
    game_init_bit7_set_boundary,
    game_init_second_source_boundary,
    revoked,
};

enum class MillenniumAtariObservedPrivilege : std::uint8_t { user, supervisor };

struct MillenniumAtariStatusRegisterObservation {
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint16_t status_register = 0;
    MillenniumAtariObservedPrivilege privilege = MillenniumAtariObservedPrivilege::user;
};

struct MillenniumAtariXbiosSelectorTwoObservation {
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    std::uint32_t trap_address = 0;
    std::uint16_t selector = 0;
    std::uint32_t result_d0 = 0;
};

struct MillenniumAtariXbiosSelectorThreeObservation {
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    std::uint32_t trap_address = 0;
    std::uint16_t selector = 0;
    std::uint32_t result_d0 = 0;
};

struct MillenniumAtariXbiosSelectorFourObservation {
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    std::uint32_t trap_address = 0;
    std::uint16_t selector = 0;
    std::uint32_t result_d0 = 0;
};

struct MillenniumAtariLineAObservation {
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t returned_a0 = 0;
    std::uint32_t value_at_a0_plus_8 = 0;
    std::uint32_t value_at_a0_plus_12 = 0;
};

struct MillenniumAtariXbiosSelector21Observation {
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    std::uint32_t trap_address = 0;
    std::uint16_t selector = 0;
    std::uint32_t result_d0 = 0;
};
using MillenniumAtariXbiosSelector6Observation = MillenniumAtariXbiosSelector21Observation;
using MillenniumAtariXbiosSelector38Observation = MillenniumAtariXbiosSelector21Observation;
struct MillenniumAtariGemdosSelector61Observation {
    std::uint64_t generation=0; std::uint64_t sequence=0;
    std::uint32_t trap_address=0; std::uint16_t selector=0;
    std::int32_t result_d0=0;
};
using MillenniumAtariGemdosSelector63Observation = MillenniumAtariGemdosSelector61Observation;
using MillenniumAtariGemdosSelector62Observation = MillenniumAtariGemdosSelector61Observation;
struct MillenniumAtariFreadPrefixObservation {
    std::uint64_t generation=0; std::uint64_t sequence=0;
    std::uint32_t first_address=0; std::uint16_t first_word=0;
    std::uint32_t second_address=0; std::uint16_t second_word=0;
};
struct MillenniumAtariGameInitSourceByteObservation {
    std::uint64_t generation=0; std::uint64_t sequence=0;
    std::uint32_t instruction_address=0; std::uint32_t source_address=0;
    std::uint8_t source_byte=0;
};
struct MillenniumAtariGameInitZeroPairObservation {
    std::uint64_t generation=0; std::uint64_t sequence=0;
    std::uint32_t instruction_address=0;
    std::uint32_t first_source_address=0; std::uint8_t first_source_byte=0;
    std::uint32_t second_source_address=0; std::uint8_t second_source_byte=0;
};
struct MillenniumAtariGameInitReplicatedByteObservation {
    std::uint64_t generation=0; std::uint64_t sequence=0;
    std::uint32_t instruction_address=0; std::uint32_t source_address=0;
    std::uint8_t source_byte=0;
};
struct MillenniumAtariGameInitSwappedPairObservation {
    std::uint64_t generation=0; std::uint64_t sequence=0;
    std::uint32_t instruction_address=0;
    std::uint32_t first_source_address=0; std::uint8_t first_source_byte=0;
    std::uint32_t second_source_address=0; std::uint8_t second_source_byte=0;
};
struct MillenniumAtariGameInitExtendedRunObservation {
    std::uint64_t generation=0; std::uint64_t sequence=0;
    std::uint32_t instruction_address=0;
    std::uint32_t count_source_address=0; std::uint8_t count_low_byte=0;
    std::uint32_t first_value_address=0; std::uint8_t first_value_byte=0;
    std::uint32_t second_value_address=0; std::uint8_t second_value_byte=0;
};
struct MillenniumAtariGameInitAlternateWrite {
    std::uint32_t address=0; std::uint16_t value=0;
    constexpr bool operator==(const MillenniumAtariGameInitAlternateWrite&) const = default;
};
struct MillenniumAtariGameInitPaletteWordsObservation {
    std::uint64_t generation=0; std::uint64_t sequence=0;
    std::uint32_t instruction_address=0;
    std::uint32_t source_address=0; std::uint32_t destination_address=0;
    std::array<std::uint16_t,16> destination_words{};
};
struct MillenniumAtariGameInitPaletteXbios6Observation {
    std::uint64_t generation=0; std::uint64_t sequence=0;
    std::uint32_t trap_address=0; std::uint16_t selector=0;
    std::uint32_t result_d0=0;
};
struct MillenniumAtariGameInitPaletteRecurrenceObservation {
    std::uint64_t generation=0; std::uint64_t sequence=0;
    std::uint32_t instruction_address=0;
    std::uint32_t source_address=0; std::uint32_t destination_address=0;
    std::array<std::uint8_t,96> source_bytes{};
    std::array<std::uint16_t,16> destination_words{};
};
struct MillenniumAtariGameInitPaletteRtsObservation {
    std::uint64_t generation=0; std::uint64_t sequence=0;
    std::uint32_t instruction_address=0;
    std::uint32_t stack_address=0; std::uint32_t return_address=0;
};

struct MillenniumAtariBchgObservation {
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t d1 = 0;
    std::uint32_t a2 = 0;
    std::uint8_t byte_before = 0;
};
struct MillenniumAtariD0IndexedByteObservation {
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t d0 = 0;
    std::uint32_t source_address = 0;
    std::uint8_t source_byte = 0;
};
struct MillenniumAtariD0IndexedWordObservation {
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t d0 = 0;
    std::uint32_t source_address = 0;
    std::uint16_t source_word = 0;
};
using MillenniumAtariA0IndexedWordObservation = MillenniumAtariD0IndexedWordObservation;
struct MillenniumAtariMovemFrameObservation { std::uint64_t generation=0; std::uint64_t sequence=0; std::uint32_t instruction_address=0; std::uint32_t frame_address=0; std::array<std::uint32_t,15> registers{}; std::uint32_t rts_return_address=0; };

struct MillenniumAtariConfigHardwareWrite {
    std::size_t order = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t address = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumAtariConfigHardwareWrite&) const = default;
};

struct MillenniumAtariConfigConsumerCheckpoint {
    std::uint64_t generation = 0;
    MillenniumAtariConfigConsumerState state =
        MillenniumAtariConfigConsumerState::revoked;
    std::uint32_t jsr_instruction_address = 0;
    std::uint32_t jsr_return_address = 0;
    std::uint32_t jsr_target_address = 0;
    std::uint16_t entry_jump_opcode = 0;
    std::uint32_t entry_jump_target_address = 0;
    std::uint32_t entry_jump_file_offset = 0;
    std::uint32_t boundary_instruction_address = 0;
    std::uint16_t boundary_opcode = 0;
    std::string boundary_dependency;
    std::string mapped_prelude_sha256;
    std::size_t local_control_transfers_executed = 0;
    bool return_address_materialized = false;
    bool status_register_read = false;
    bool hardware_write_executed = false;
    std::uint64_t last_sequence = 0;
    std::uint16_t observed_status_register = 0;
    MillenniumAtariObservedPrivilege observed_privilege =
        MillenniumAtariObservedPrivilege::user;
    bool supervisor_bit_was_set = false;
    bool branch_taken = false;
    std::uint16_t resulting_status_register = 0;
    std::uint32_t converged_jsr_address = 0;
    std::uint32_t converged_jsr_target = 0;
    std::uint32_t converged_jsr_return_address = 0;
    std::uint32_t xbios_trap_address = 0;
    std::uint16_t xbios_selector = 0;
    std::size_t local_instruction_count = 0;
    std::vector<MillenniumAtariConfigHardwareWrite> hardware_writes;
    bool selector_two_result_observed = false;
    std::uint32_t selector_two_result_d0 = 0;
    std::uint32_t selector_two_store_address = 0;
    std::uint32_t selector_two_stack_cleanup_bytes = 0;
    std::string selector_two_continuation_sha256;
    bool selector_three_result_observed = false;
    std::uint32_t selector_three_result_d0 = 0;
    std::uint32_t selector_three_store_address = 0;
    std::uint32_t selector_three_stack_cleanup_bytes = 0;
    std::string selector_three_continuation_sha256;
    bool selector_four_result_observed = false;
    std::uint16_t selector_four_result_d0_word = 0;
    std::uint32_t selector_four_store_address = 0;
    std::uint32_t selector_four_stack_cleanup_bytes = 0;
    std::uint32_t line_a_init_address = 0;
    std::uint16_t line_a_init_opcode = 0;
    std::string selector_four_continuation_sha256;
    bool line_a_result_observed = false;
    std::uint32_t line_a_returned_a0 = 0;
    std::uint32_t line_a_result_a3 = 0;
    std::uint32_t line_a_result_a4 = 0;
    std::uint32_t line_a_a3_store_address = 0;
    std::uint32_t line_a_a4_store_address = 0;
    std::string line_a_continuation_sha256;
    std::string line_a_caller_continuation_sha256;
    bool selector_21_result_observed = false;
    std::uint32_t selector_21_result_d0 = 0;
    std::uint32_t selector_21_stack_cleanup_bytes = 0;
    std::uint32_t selector_6_pointer_argument = 0;
    std::string selector_21_continuation_sha256;
    bool selector_6_result_observed = false;
    std::uint32_t selector_6_result_d0 = 0;
    std::uint32_t selector_6_stack_cleanup_bytes = 0;
    std::uint32_t next_jsr_address = 0;
    std::uint32_t next_jsr_target = 0;
    std::string selector_6_continuation_sha256;
    std::string jsr_2b55a_prefix_sha256;
    std::uint32_t bsr_instruction_address = 0;
    std::uint32_t bsr_target = 0;
    std::uint32_t bsr_return_address = 0;
    std::uint32_t callee_a3 = 0;
    std::uint32_t callee_clear_address = 0;
    std::uint32_t indexed_instruction_address = 0;
    std::string bsr_2b59a_prefix_sha256;
    std::uint32_t indexed_source_base = 0;
    std::uint32_t indexed_source_address = 0;
    std::uint8_t indexed_source_byte = 0;
    std::uint32_t indexed_first_destination = 0;
    std::uint32_t indexed_second_destination = 0;
    std::string indexed_write_sha256;
    std::uint32_t setup_a1 = 0;
    std::uint32_t setup_a0_first = 0;
    std::uint32_t setup_a0_second = 0;
    std::uint32_t setup_d7 = 0;
    std::uint32_t indexed_word_instruction_address = 0;
    std::string a1_setup_sha256;
    std::uint32_t indexed_word_source_address = 0;
    std::uint16_t indexed_word_value = 0;
    std::uint32_t a0_indexed_instruction_address = 0;
    std::string indexed_word_sha256;
    std::uint16_t a0_indexed_word_value = 0;
    std::uint32_t loop_a0_value = 0;
    std::uint16_t loop_d0_value = 0;
    std::uint16_t loop_d7_value = 0;
    std::uint32_t loop_branch_target = 0;
    std::string a0_indexed_tail_sha256;
    std::uint32_t loop_iteration = 0;
    std::uint32_t loop_current_a1 = 0;
    std::string loop_setup_sha256;
    std::string loop_epilogue_sha256;
    std::uint32_t movem_instruction_address = 0;
    std::uint16_t movem_register_mask = 0;
    bool movem_frame_observed=false; std::uint32_t movem_frame_address=0;
    std::array<std::uint32_t,15> restored_registers{};
    std::uint32_t restored_stack_address=0; std::uint32_t rts_return_address=0;
    std::string movem_rts_sha256; std::string caller_jsr_2aa68_sha256;
    std::uint32_t selector_38_pointer_argument = 0;
    std::string jsr_2aa68_prefix_sha256;
    bool selector_38_result_observed = false;
    std::uint32_t selector_38_result_d0 = 0;
    std::uint32_t selector_38_stack_cleanup_bytes = 0;
    std::uint32_t caller_d7 = 0;
    std::string selector_38_return_sha256;
    std::string selector_38_caller_sha256;
    std::uint32_t gemdos_trap_address = 0;
    std::uint16_t gemdos_selector = 0;
    std::uint16_t gemdos_open_mode = 0;
    std::uint32_t gemdos_filename_pointer = 0;
    std::string caller_jsr_2a5aa_sha256;
    std::string gemdos_61_prefix_sha256;
    bool gemdos_61_result_observed=false;
    std::int32_t gemdos_61_result_d0=0;
    std::uint32_t gemdos_handle_store_address=0;
    std::uint32_t gemdos_stack_cleanup_bytes=0;
    std::uint32_t fopen_branch_address=0;
    std::uint32_t fopen_branch_target=0;
    std::uint32_t fopen_positive_d0=0;
    std::uint32_t fopen_positive_d1=0;
    std::string gemdos_61_return_sha256;
    std::string fopen_caller_branch_sha256;
    std::uint32_t gemdos_63_trap_address=0;
    std::uint16_t gemdos_63_selector=0;
    std::uint16_t gemdos_63_handle=0;
    std::uint32_t gemdos_63_buffer=0;
    std::uint32_t gemdos_63_count=0;
    std::string gemdos_63_prefix_sha256;
    bool gemdos_63_result_observed=false;
    std::int32_t gemdos_63_result_d0=0;
    std::uint32_t gemdos_63_stack_cleanup_bytes=0;
    std::string gemdos_63_return_sha256;
    std::string fread_caller_jump_sha256;
    std::uint32_t gemdos_62_trap_address=0;
    std::uint16_t gemdos_62_selector=0;
    std::uint16_t gemdos_62_handle=0;
    std::string gemdos_62_prefix_sha256;
    bool gemdos_62_result_observed=false;
    std::int32_t gemdos_62_result_d0=0;
    std::uint32_t gemdos_62_stack_cleanup_bytes=0;
    std::string gemdos_62_return_sha256;
    std::uint32_t fread_prefix_a4=0;
    bool fread_prefix_observed=false;
    std::uint16_t fread_prefix_d6=0;
    std::uint16_t fread_prefix_d7=0;
    std::uint32_t caller_a5=0;
    std::string fread_caller_prefix_sha256;
    std::uint32_t game_init_a3=0;
    std::uint16_t game_init_d6=0;
    std::uint16_t game_init_d7=0;
    std::uint32_t game_init_a0=0;
    std::uint32_t game_init_a6=0;
    std::uint16_t game_init_d5=0;
    std::uint16_t game_init_d2=0;
    std::uint32_t game_init_source_address=0;
    std::string game_init_dispatch_sha256;
    bool game_init_source_observed=false;
    std::uint8_t game_init_source_byte=0;
    std::uint32_t game_init_next_instruction=0;
    std::string game_init_source_dispatch_sha256;
    std::string game_init_nonzero_dispatch_sha256;
    bool game_init_zero_pair_observed=false;
    std::uint8_t game_init_zero_first_byte=0;
    std::uint8_t game_init_zero_second_byte=0;
    std::uint32_t game_init_zero_destination_address=0;
    std::string game_init_zero_pair_prefix_sha256;
    std::string game_init_zero_counter_continuation_sha256;
    std::uint32_t game_init_completed_planes=0;
    std::string game_init_alternate_run_sha256;
    std::vector<std::uint32_t> game_init_alternate_source_addresses;
    std::vector<std::uint8_t> game_init_alternate_source_bytes;
    std::vector<MillenniumAtariGameInitAlternateWrite> game_init_alternate_writes;
    std::string game_init_caller_2b448_sha256;
    std::string game_init_palette_copy_prefix_sha256;
    std::string game_init_palette_source_sha256;
    std::array<std::uint32_t,24> game_init_palette_source_longs{};
    std::uint32_t game_init_palette_clear_destination=0;
    std::uint32_t game_init_palette_copy_destination=0;
    std::string game_init_palette_arithmetic_sha256;
    std::array<std::uint16_t,16> game_init_palette_result_words{};
    std::array<std::uint8_t,48> game_init_palette_result_bytes{};
    std::uint32_t game_init_palette_xbios_trap_address=0;
    std::uint16_t game_init_palette_xbios_selector=0;
    std::uint32_t game_init_palette_xbios_pointer=0;
    bool game_init_palette_xbios_result_observed=false;
    std::uint32_t game_init_palette_xbios_result_d0=0;
    std::uint32_t game_init_palette_xbios_stack_cleanup_bytes=0;
    std::string game_init_palette_post_xbios_sha256;
    std::uint32_t game_init_palette_delay_initial_d0=0;
    std::uint32_t game_init_palette_delay_iterations=0;
    std::uint32_t game_init_palette_delay_final_d0=0;
    std::uint32_t game_init_palette_outer_backedge_address=0;
    std::string game_init_palette_recurrence_sha256;
    std::uint32_t game_init_palette_completed_passes=0;
    std::uint32_t game_init_palette_terminal_trap_address=0;
    std::uint32_t game_init_palette_terminal_result_d0=0;
    std::string game_init_palette_terminal_sha256;
    std::uint32_t game_init_palette_rts_address=0;
    std::uint32_t game_init_palette_rts_stack_address=0;
    std::uint32_t game_init_palette_rts_return_address=0;
    std::string game_init_palette_caller_continuation_sha256;
    bool game_init_second_config_open=false;
};

struct MillenniumAtariConfigConsumerResult {
    bool accepted = false;
    std::string error;
};

// Executes the caller-connected JSR and the config file's absolute JMP. The
// following MOVE SR,D0 depends on original CPU privilege/status state, so the
// session stops before that instruction and does not choose either branch or
// perform the fall-through hardware writes.
class MillenniumAtariConfigConsumerSession {
public:
    MillenniumAtariConfigConsumerSession(std::uint64_t generation,
        const NativeRuntimeMemory& memory,
        const MillenniumAtariReadOnlyGemdosCheckpoint& gemdos,
        const MillenniumAtariFreadConfigLoadAddressBoundary& load_boundary,
        const MillenniumAtariFreadMappedConfigPrelude& prelude);

    [[nodiscard]] const MillenniumAtariConfigConsumerCheckpoint& checkpoint() const noexcept {
        return checkpoint_;
    }
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_status_register(
        const MillenniumAtariStatusRegisterObservation& observation);
    [[nodiscard]] std::vector<NativeRuntimeEffectBatch> make_hardware_effect_batches(
        std::string id_prefix) const;
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_xbios_selector_two(
        const MillenniumAtariXbiosSelectorTwoObservation& observation);
    [[nodiscard]] NativeRuntimeEffectBatch make_selector_two_result_effect_batch(
        std::string id) const;
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_xbios_selector_three(
        const MillenniumAtariXbiosSelectorThreeObservation& observation);
    [[nodiscard]] NativeRuntimeEffectBatch make_selector_three_result_effect_batch(
        std::string id) const;
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_xbios_selector_four(
        const MillenniumAtariXbiosSelectorFourObservation& observation);
    [[nodiscard]] NativeRuntimeEffectBatch make_selector_four_result_effect_batch(
        std::string id) const;
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_line_a(
        const MillenniumAtariLineAObservation& observation);
    [[nodiscard]] NativeRuntimeEffectBatch make_line_a_result_effect_batch(
        std::string id) const;
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_xbios_selector_21(
        const MillenniumAtariXbiosSelector21Observation& observation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_xbios_selector_6(
        const MillenniumAtariXbiosSelector6Observation& observation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_bchg_2b55a(
        const MillenniumAtariBchgObservation& observation);
    [[nodiscard]] NativeRuntimeEffectBatch make_bchg_effect_batch(std::string id) const;
    [[nodiscard]] MillenniumAtariConfigConsumerResult execute_jsr_2b55a();
    [[nodiscard]] MillenniumAtariConfigConsumerResult execute_bsr_2b59a();
    [[nodiscard]] NativeRuntimeEffectBatch make_bsr_2b59a_effect_batch(std::string id) const;
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_d0_indexed_byte(
        const MillenniumAtariD0IndexedByteObservation& observation);
    [[nodiscard]] NativeRuntimeEffectBatch make_d0_indexed_effect_batch(std::string id) const;
    [[nodiscard]] MillenniumAtariConfigConsumerResult execute_a1_setup();
    [[nodiscard]] NativeRuntimeEffectBatch make_a1_setup_effect_batch(std::string id) const;
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_d0_indexed_word(
        const MillenniumAtariD0IndexedWordObservation& observation);
    [[nodiscard]] NativeRuntimeEffectBatch make_d0_indexed_word_effect_batch(std::string id) const;
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_a0_indexed_word(
        const MillenniumAtariA0IndexedWordObservation& observation);
    [[nodiscard]] NativeRuntimeEffectBatch make_a0_indexed_tail_effect_batch(std::string id) const;
    [[nodiscard]] MillenniumAtariConfigConsumerResult execute_loop_iteration_setup();
    [[nodiscard]] NativeRuntimeEffectBatch make_loop_iteration_setup_effect_batch(std::string id) const;
    [[nodiscard]] MillenniumAtariConfigConsumerResult execute_loop_epilogue();
    [[nodiscard]] NativeRuntimeEffectBatch make_loop_epilogue_effect_batch(std::string id) const;
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_movem_frame(const MillenniumAtariMovemFrameObservation& observation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult execute_jsr_2aa68();
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_xbios_selector_38(
        const MillenniumAtariXbiosSelector38Observation& observation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult execute_jsr_2aa0c();
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_gemdos_selector_61(
        const MillenniumAtariGemdosSelector61Observation& observation);
    [[nodiscard]] NativeRuntimeEffectBatch make_gemdos_selector_61_effect_batch(std::string id) const;
    [[nodiscard]] MillenniumAtariConfigConsumerResult execute_jsr_2a5c2();
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_gemdos_selector_63(
        const MillenniumAtariGemdosSelector63Observation& observation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_gemdos_selector_62(
        const MillenniumAtariGemdosSelector62Observation& observation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_fread_prefix(
        const MillenniumAtariFreadPrefixObservation& observation);
    [[nodiscard]] NativeRuntimeEffectBatch make_fread_prefix_effect_batch(std::string id) const;
    [[nodiscard]] MillenniumAtariConfigConsumerResult execute_jsr_2b2be();
    [[nodiscard]] NativeRuntimeEffectBatch make_game_init_setup_effect_batch(std::string id) const;
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_game_init_source_byte(
        const MillenniumAtariGameInitSourceByteObservation& observation);
    [[nodiscard]] NativeRuntimeEffectBatch make_game_init_source_byte_effect_batch(std::string id) const;
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_game_init_zero_pair(
        const MillenniumAtariGameInitZeroPairObservation& observation);
    [[nodiscard]] NativeRuntimeEffectBatch make_game_init_zero_pair_effect_batch(std::string id) const;
    [[nodiscard]] MillenniumAtariConfigConsumerResult execute_game_init_zero_counter_branch();
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_game_init_replicated_byte(
        const MillenniumAtariGameInitReplicatedByteObservation& observation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_game_init_swapped_pair(
        const MillenniumAtariGameInitSwappedPairObservation& observation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_game_init_extended_run(
        const MillenniumAtariGameInitExtendedRunObservation& observation);
    [[nodiscard]] NativeRuntimeEffectBatch make_game_init_alternate_effect_batch(std::string id) const;
    [[nodiscard]] MillenniumAtariConfigConsumerResult execute_game_init_return();
    [[nodiscard]] MillenniumAtariConfigConsumerResult execute_game_init_palette_copy_prefix();
    [[nodiscard]] NativeRuntimeEffectBatch make_game_init_palette_copy_effect_batch(std::string id) const;
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_game_init_palette_words(
        const MillenniumAtariGameInitPaletteWordsObservation& observation);
    [[nodiscard]] NativeRuntimeEffectBatch make_game_init_palette_arithmetic_effect_batch(std::string id) const;
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_game_init_palette_xbios_selector_6(
        const MillenniumAtariGameInitPaletteXbios6Observation& observation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_game_init_palette_recurrence(
        const MillenniumAtariGameInitPaletteRecurrenceObservation& observation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_game_init_palette_rts(
        const MillenniumAtariGameInitPaletteRtsObservation& observation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_game_init_second_config_fopen(
        const MillenniumAtariGemdosSelector61Observation& observation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult revoke(std::uint64_t generation);

private:
    void execute_game_init_alternate_run(std::uint16_t value, std::uint32_t source_advance_after_run);
    MillenniumAtariConfigConsumerCheckpoint checkpoint_;
};

} // namespace eon
