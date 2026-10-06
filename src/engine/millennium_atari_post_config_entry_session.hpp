#pragma once

#include "data/atari_st_prg.hpp"
#include "engine/millennium_atari_config_consumer_session.hpp"
#include "engine/native_runtime_memory.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace eon {

enum class MillenniumAtariPostConfigEntryState : std::uint8_t {
    entry_jump_boundary,
    status_register_boundary,
    xbios_trap_boundary,
    xbios_selector_15_boundary,
    xbios_selector_2_boundary,
    xbios_selector_3_boundary,
    xbios_selector_4_boundary,
    line_a_instruction_boundary,
    post_line_a_subroutine_boundary,
    caller_transfer_boundary,
    bytecode_command_boundary,
    local_call_trap_boundary,
    crawcin_flag_branch_observed,
    gemdos_fopen_boundary,
    gemdos_fopen_result_observed,
    gemdos_fcreate_result_observed,
};

struct MillenniumAtariPostConfigEntryJumpObservation {
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    std::uint32_t instruction_address = 0;
};

struct MillenniumAtariPostConfigEntryHardwareWrite {
    std::size_t order = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t address = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumAtariPostConfigEntryHardwareWrite&) const = default;
};

struct MillenniumAtariPostConfigMemoryEffect {
    std::uint32_t address = 0;
    std::uint8_t before = 0;
    std::uint8_t after = 0;
    constexpr bool operator==(const MillenniumAtariPostConfigMemoryEffect&) const = default;
};

// Full external trace for the selector-$26 return. Register and memory effects
// remain observations; the runtime does not invent an XBIOS result or emulate
// firmware behavior.
struct MillenniumAtariPostConfigXbiosReturnObservation {
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    std::uint32_t trap_address = 0;
    std::uint16_t selector = 0;
    std::uint32_t argument_stack_address = 0;
    std::array<std::uint8_t, 6> argument_bytes{};
    std::uint32_t post_service_pc = 0;
    std::uint16_t post_service_sr = 0;
    std::array<std::uint32_t, 8> post_service_data{};
    std::array<std::uint32_t, 8> post_service_address{};
    std::uint32_t rts_stack_address = 0;
    std::array<std::uint8_t, 4> rts_stack_bytes{};
    std::vector<MillenniumAtariPostConfigMemoryEffect> memory_effects;
};

// Exact post-service facts for the selector-$15 / $2 / $3 / $4 calls in the
// admitted local continuation. No selector meaning is assigned. D0 and A7
// are the only returned registers consumed by the following verified code.
struct MillenniumAtariPostConfigXbiosResultObservation {
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    std::uint32_t trap_address = 0;
    std::uint16_t selector = 0;
    std::uint32_t argument_stack_address = 0;
    std::vector<std::uint8_t> argument_bytes;
    std::uint32_t post_service_pc = 0;
    std::uint32_t post_service_a7 = 0;
    std::uint32_t result_d0 = 0;
};

struct MillenniumAtariPostConfigLineAMemoryRead {
    std::uint32_t address = 0;
    std::array<std::uint8_t, 4> bytes{};
};

// Opaque Line-A return state plus the two memory operands consumed by the
// hash-verified local suffix. This records what the external handler did; it
// neither implements Line-A nor assigns meaning to A0 or the pointed words.
struct MillenniumAtariPostConfigLineAObservation {
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint16_t instruction_opcode = 0;
    std::uint32_t post_service_pc = 0;
    std::uint16_t post_service_sr = 0;
    std::array<std::uint32_t, 8> post_service_data{};
    std::array<std::uint32_t, 8> post_service_address{};
    std::array<MillenniumAtariPostConfigLineAMemoryRead, 2> memory_reads{};
    std::uint32_t rts_stack_address = 0;
    std::array<std::uint8_t, 4> rts_stack_bytes{};
};

// Entry trace for the local $2340c callee. Its first verified block copies a
// bounded source range supplied as observed bytes, then dispatches the first
// in-module bytecode command. The remaining local macro/raster path is
// executed separately up to GEMDOS TRAP #1 selector 7.
struct MillenniumAtariPostConfigLocalCallObservation {
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    std::uint32_t instruction_address = 0;
    std::array<std::uint32_t, 8> data_registers{};
    std::array<std::uint32_t, 8> address_registers{};
    std::uint32_t rts_stack_address = 0;
    std::array<std::uint8_t, 4> rts_stack_bytes{};
    std::uint32_t copy_source_address = 0;
    std::vector<std::uint8_t> copy_source_bytes;
};

// Observation of the existing $1fc6e TST.B/BEQ after the caller returns from
// GEMDOS. The byte and resulting PC are runtime facts; the session does not
// emulate the GEMDOS read, map D0 to a key, or execute either successor.
struct MillenniumAtariPostConfigCrawcinBranchObservation {
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t flag_address = 0;
    std::uint8_t flag_value = 0;
    std::uint32_t observed_next_pc = 0;
};

// External execution stop at the hash-verified GEMDOS Fopen TRAP. The
// argument bytes are observations from the running Atari, not a host result.
struct MillenniumAtariPostConfigFopenObservation {
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    std::uint32_t trap_address = 0;
    std::uint32_t observed_a7 = 0;
    std::uint32_t argument_stack_address = 0;
    std::array<std::uint8_t, 8> argument_bytes{};
};

// Observed GEMDOS result and execution after the hash-bound local return
// suffix. D0, SR, and the post-RTS PC/A7 are external trace observations;
// the intermediate cleanup/store/TST/RTS effects are checked from the exact
// module bytes and owned stack.
struct MillenniumAtariPostConfigFopenReturnObservation {
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    std::uint32_t trap_address = 0;
    std::uint32_t post_service_pc = 0;
    std::uint32_t post_service_a7 = 0;
    std::uint16_t post_service_sr = 0;
    std::uint32_t result_d0 = 0;
    std::uint32_t post_rts_pc = 0;
    std::uint32_t post_rts_a7 = 0;
};

// Native boundary for the immediately following GEMDOS Fcreate call. The
// argument bytes are observations from the running Atari; this session never
// invokes GEMDOS or creates a host/original-media file.
struct MillenniumAtariPostConfigFcreateObservation {
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    std::uint32_t trap_address = 0;
    std::uint32_t observed_a7 = 0;
    std::uint32_t argument_stack_address = 0;
    std::array<std::uint8_t, 8> argument_bytes{};
};

// Raw GEMDOS Fcreate result plus the observed return from the local suffix.
// The result remains opaque; no host file operation is performed.
struct MillenniumAtariPostConfigFcreateReturnObservation {
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    std::uint32_t trap_address = 0;
    std::uint32_t post_service_pc = 0;
    std::uint32_t post_service_a7 = 0;
    std::uint16_t post_service_sr = 0;
    std::uint32_t result_d0 = 0;
    std::uint32_t post_rts_pc = 0;
    std::uint32_t post_rts_a7 = 0;
};

struct MillenniumAtariPostConfigEntryCheckpoint {
    std::uint64_t generation = 0;
    std::uint64_t last_sequence = 0;
    MillenniumAtariPostConfigEntryState state =
        MillenniumAtariPostConfigEntryState::entry_jump_boundary;
    std::uint32_t entry_jump_address = 0x11e00;
    std::uint32_t entry_jump_target = 0x1c62c;
    std::uint32_t status_register_address = 0x1c62c;
    bool entry_jump_executed = false;
    bool entry_jump_observed = false;
    bool status_register_observed = false;
    std::uint16_t observed_status_register = 0;
    MillenniumAtariObservedPrivilege observed_privilege = MillenniumAtariObservedPrivilege::user;
    bool supervisor_branch_taken = false;
    std::uint16_t resulting_data_register = 0;
    std::uint16_t resulting_status_register = 0;
    std::uint32_t local_jsr_return_address = 0;
    bool local_jsr_return_address_materialized = false;
    std::uint32_t xbios_trap_address = 0;
    std::uint16_t xbios_selector = 0;
    std::uint32_t xbios_pointer_argument = 0;
    std::int32_t relative_stack_delta = 0;
    std::vector<std::uint8_t> relative_stack_bytes;
    bool xbios_26_return_observed = false;
    std::uint32_t xbios_26_trap_address = 0;
    std::uint16_t xbios_26_selector = 0;
    std::uint32_t xbios_26_argument_stack_address = 0;
    std::array<std::uint8_t, 6> xbios_26_argument_bytes{};
    std::uint32_t xbios_26_post_service_pc = 0;
    std::uint16_t xbios_26_post_service_sr = 0;
    std::array<std::uint32_t, 8> xbios_26_post_service_data{};
    std::array<std::uint32_t, 8> xbios_26_post_service_address{};
    std::uint32_t xbios_26_rts_stack_address = 0;
    std::uint32_t xbios_26_rts_return_address = 0;
    std::vector<MillenniumAtariPostConfigMemoryEffect> xbios_26_memory_effects;
    std::uint32_t selector_15_trap_address = 0;
    std::uint32_t selector_15_argument_stack_address = 0;
    std::array<std::uint8_t, 6> selector_15_argument_bytes{};
    bool selector_15_return_observed = false;
    std::uint32_t selector_15_result_d0 = 0;
    std::uint32_t post_config_call_stack_address = 0;
    std::uint32_t post_config_result_argument_stack_address = 0;
    std::uint32_t post_config_result_trap_address = 0;
    std::uint16_t post_config_result_selector = 0;
    std::vector<std::uint8_t> post_config_result_argument_bytes;
    std::array<std::uint32_t, 3> post_config_raw_result_d0{};
    std::uint32_t post_config_result_write_address = 0;
    std::uint32_t post_config_result_write_value = 0;
    std::uint8_t post_config_result_write_width = 0;
    std::uint32_t line_a_instruction_address = 0;
    std::uint16_t line_a_instruction_opcode = 0;
    bool line_a_return_observed = false;
    std::uint32_t line_a_post_service_pc = 0;
    std::uint16_t line_a_post_service_sr = 0;
    std::array<std::uint32_t, 8> line_a_post_service_data{};
    std::array<std::uint32_t, 8> line_a_post_service_address{};
    std::array<MillenniumAtariPostConfigLineAMemoryRead, 2> line_a_memory_reads{};
    std::uint32_t line_a_rts_stack_address = 0;
    std::uint32_t line_a_rts_return_address = 0;
    std::uint32_t line_a_result_a3 = 0;
    std::uint32_t line_a_result_a4 = 0;
    std::uint32_t line_a_local_return_pc = 0;
    std::uint32_t caller_compare_d6 = 0;
    bool caller_compare_equal = false;
    std::uint32_t caller_final_d1 = 0;
    std::uint32_t caller_stack_reset_address = 0;
    std::uint32_t first_local_jsr_address = 0;
    std::uint32_t first_local_jsr_return_address = 0;
    std::uint32_t first_local_jsr_stack_address = 0;
    std::uint32_t next_local_call_boundary_address = 0;
    bool local_call_observed = false;
    std::uint32_t local_call_entry_address = 0;
    std::uint32_t local_call_entry_return_address = 0;
    std::array<std::uint32_t, 8> local_call_entry_data{};
    std::array<std::uint32_t, 8> local_call_entry_address_registers{};
    std::uint32_t local_call_copy_source_address = 0;
    std::uint32_t local_call_copy_destination_address = 0;
    std::size_t local_call_copy_byte_count = 0;
    std::vector<std::uint8_t> local_call_copy_source_bytes;
    std::uint32_t local_call_macro_address = 0;
    std::uint32_t local_call_macro_next_address = 0;
    std::uint32_t local_call_macro_store_value = 0;
    std::uint32_t bytecode_next_address = 0;
    std::uint32_t bytecode_dispatch_return_address = 0;
    std::vector<MillenniumAtariPostConfigMemoryEffect> bytecode_continuation_effects;
    std::uint32_t bytecode_final_screen_pointer = 0;
    std::uint32_t bytecode_mask_pointer = 0;
    std::uint32_t bytecode_secondary_mask_pointer = 0;
    std::uint32_t local_call_trap_address = 0;
    std::uint16_t local_call_trap_selector = 0;
    std::uint32_t local_call_trap_stack_address = 0;
    std::uint32_t local_call_return_address = 0;
    std::vector<MillenniumAtariPostConfigEntryHardwareWrite> hardware_writes;
    bool crawcin_flag_branch_observed = false;
    std::uint8_t crawcin_flag_value = 0;
    std::uint32_t crawcin_flag_branch_next_pc = 0;
    bool gemdos_fopen_observed = false;
    std::uint32_t gemdos_fopen_trap_address = 0;
    std::uint32_t gemdos_fopen_argument_stack_address = 0;
    std::array<std::uint8_t, 8> gemdos_fopen_argument_bytes{};
    bool gemdos_fopen_result_observed = false;
    std::uint32_t gemdos_fopen_result_d0 = 0;
    std::uint16_t gemdos_fopen_post_service_sr = 0;
    std::uint16_t gemdos_fopen_result_sr = 0;
    std::uint32_t gemdos_fopen_return_pc = 0;
    std::uint32_t gemdos_fopen_result_a7 = 0;
    std::uint16_t gemdos_fopen_stored_word = 0;
    std::uint32_t gemdos_fopen_suffix_instruction_count = 0;
    bool gemdos_fcreate_boundary_prepared = false;
    bool gemdos_fcreate_observed = false;
    std::uint32_t gemdos_fcreate_trap_address = 0;
    std::uint32_t gemdos_fcreate_argument_stack_address = 0;
    std::array<std::uint8_t, 8> gemdos_fcreate_argument_bytes{};
    std::uint32_t gemdos_fcreate_stack_base = 0;
    std::uint32_t gemdos_fcreate_data_register_7 = 0;
    bool gemdos_fcreate_result_observed = false;
    std::uint32_t gemdos_fcreate_result_d0 = 0;
    std::uint16_t gemdos_fcreate_post_service_sr = 0;
    std::uint16_t gemdos_fcreate_result_sr = 0;
    std::uint32_t gemdos_fcreate_return_pc = 0;
    std::uint32_t gemdos_fcreate_result_a7 = 0;
    std::uint16_t gemdos_fcreate_stored_word = 0;
    std::uint32_t gemdos_fcreate_suffix_instruction_count = 0;
};

struct MillenniumAtariPostConfigEntryResult {
    bool accepted = false;
    std::string error;
};

// Executes only the verified MILL22B.INF entry jump and the fixed local
// prologue through its first XBIOS trap. The caller must first observe the
// loader RTS returning to $11e00, and native memory must contain the exact
// entry bytes. Status/privilege remain typed observations. The session never
// invokes XBIOS or invents an A7 address.
class MillenniumAtariPostConfigEntrySession {
public:
    MillenniumAtariPostConfigEntrySession(std::uint64_t generation,
        const MillenniumAtariPostConfigModuleEntryEvidence& evidence,
        std::span<const std::uint8_t> module);

    [[nodiscard]] const MillenniumAtariPostConfigEntryCheckpoint& checkpoint() const {
        return checkpoint_;
    }
    [[nodiscard]] MillenniumAtariPostConfigEntryResult observe_entry_jump(
        const MillenniumAtariPostConfigEntryJumpObservation& observation,
        const NativeRuntimeMemory& memory);
    [[nodiscard]] MillenniumAtariPostConfigEntryResult execute_entry_jump(
        const NativeRuntimeMemory& memory, std::uint64_t transfer_sequence,
        std::uint32_t transfer_return_address);
    [[nodiscard]] MillenniumAtariPostConfigEntryResult observe_status_register(
        const MillenniumAtariStatusRegisterObservation& observation,
        const NativeRuntimeMemory& memory);
    [[nodiscard]] MillenniumAtariPostConfigEntryResult observe_xbios_26_return(
        const MillenniumAtariPostConfigXbiosReturnObservation& observation);
    [[nodiscard]] MillenniumAtariPostConfigEntryResult observe_xbios_result(
        const MillenniumAtariPostConfigXbiosResultObservation& observation,
        const NativeRuntimeMemory& memory);
    [[nodiscard]] MillenniumAtariPostConfigEntryResult observe_line_a_return(
        const MillenniumAtariPostConfigLineAObservation& observation,
        const NativeRuntimeMemory& memory);
    [[nodiscard]] MillenniumAtariPostConfigEntryResult observe_local_call_2340c(
        const MillenniumAtariPostConfigLocalCallObservation& observation,
        const NativeRuntimeMemory& memory);
    [[nodiscard]] MillenniumAtariPostConfigEntryResult execute_bytecode_continuation(
        const NativeRuntimeMemory& memory);
    [[nodiscard]] MillenniumAtariPostConfigEntryResult observe_crawcin_flag_branch(
        const MillenniumAtariPostConfigCrawcinBranchObservation& observation,
        const NativeRuntimeMemory& memory);
    [[nodiscard]] MillenniumAtariPostConfigEntryResult observe_gemdos_fopen(
        const MillenniumAtariPostConfigFopenObservation& observation,
        const NativeRuntimeMemory& memory);
    [[nodiscard]] MillenniumAtariPostConfigEntryResult observe_gemdos_fopen_return(
        const MillenniumAtariPostConfigFopenReturnObservation& observation,
        const NativeRuntimeMemory& memory);
    [[nodiscard]] MillenniumAtariPostConfigEntryResult observe_gemdos_fcreate(
        const MillenniumAtariPostConfigFcreateObservation& observation,
        const NativeRuntimeMemory& memory);
    [[nodiscard]] MillenniumAtariPostConfigEntryResult observe_gemdos_fcreate_return(
        const MillenniumAtariPostConfigFcreateReturnObservation& observation,
        const NativeRuntimeMemory& memory);
    [[nodiscard]] NativeRuntimeEffectBatch make_hardware_effect_batch(std::string id) const;
    [[nodiscard]] NativeRuntimeEffectBatch make_xbios_result_effect_batch(std::string id) const;
    [[nodiscard]] NativeRuntimeEffectBatch make_line_a_continuation_effect_batch(
        std::string id) const;
    [[nodiscard]] NativeRuntimeEffectBatch make_local_call_2340c_effect_batch(
        std::string id) const;
    [[nodiscard]] NativeRuntimeEffectBatch make_bytecode_continuation_effect_batch(
        std::string id) const;
    [[nodiscard]] NativeRuntimeEffectBatch make_gemdos_fopen_result_effect_batch(
        std::string id) const;
    [[nodiscard]] NativeRuntimeEffectBatch make_gemdos_fcreate_result_effect_batch(
        std::string id) const;

private:
    std::vector<std::uint8_t> module_;
    MillenniumAtariPostConfigEntryCheckpoint checkpoint_;
};

} // namespace eon
