#pragma once

#include "data/atari_st_prg.hpp"
#include "engine/millennium_atari_read_only_gemdos_session.hpp"
#include "engine/native_runtime_memory.hpp"

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

struct MillenniumAtariBchgObservation {
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t d1 = 0;
    std::uint32_t a2 = 0;
    std::uint8_t byte_before = 0;
};

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
    [[nodiscard]] MillenniumAtariConfigConsumerResult revoke(std::uint64_t generation);

private:
    MillenniumAtariConfigConsumerCheckpoint checkpoint_;
};

} // namespace eon
