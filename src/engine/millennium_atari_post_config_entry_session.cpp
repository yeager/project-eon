#include "engine/millennium_atari_post_config_entry_session.hpp"

#include "data/m68k_executor.hpp"
#include "data/sha256.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <utility>

namespace eon {

MillenniumAtariPostConfigEntrySession::MillenniumAtariPostConfigEntrySession(
    const std::uint64_t generation,
    const MillenniumAtariPostConfigModuleEntryEvidence& evidence,
    const std::span<const std::uint8_t> module)
    : module_(module.begin(), module.end()) {
    if (generation == 0 || evidence.load_address != 0x11e00
        || evidence.initial_jump_address != 0x1c62c
        || evidence.initial_jump_file_offset != 0xa82c
        || evidence.entry_prologue_bytes != 24U
        || evidence.file_sha256 != "e315b0ec01f2fe429fdce101765577b893d031389c540de1fbe43eca121d53e9"
        || module_.size() != 84720U
        || to_hex(sha256(module_)) != evidence.file_sha256
        || module_.size() < evidence.initial_jump_file_offset + 24U) {
        throw std::runtime_error("Unsupported Millennium Atari post-config entry session");
    }
    const auto actual = parse_millennium_atari_post_config_module_entry(module_);
    if (actual.file_sha256 != evidence.file_sha256
        || actual.initial_jump_address != evidence.initial_jump_address
        || actual.initial_jump_file_offset != evidence.initial_jump_file_offset
        || actual.entry_prologue_sha256 != evidence.entry_prologue_sha256) {
        throw std::runtime_error("Millennium Atari post-config entry evidence mismatch");
    }
    checkpoint_.generation = generation;
}

MillenniumAtariPostConfigEntryResult
MillenniumAtariPostConfigEntrySession::observe_entry_jump(
    const MillenniumAtariPostConfigEntryJumpObservation& observation,
    const NativeRuntimeMemory& memory) {
    if (checkpoint_.state != MillenniumAtariPostConfigEntryState::entry_jump_boundary) {
        return {false, "MILL22B.INF entry jump is unavailable"};
    }
    if (observation.generation != checkpoint_.generation || observation.sequence == 0
        || observation.instruction_address != checkpoint_.entry_jump_address) {
        return {false, "MILL22B.INF entry jump observation is stale or at the wrong address"};
    }
    for (std::size_t index = 0; index < 6; ++index) {
        const auto byte = memory.read_byte({NativeRuntimeAddressSpace::linear, std::nullopt,
            static_cast<std::uint64_t>(checkpoint_.entry_jump_address) + index});
        if (!byte || *byte != module_[index]) {
            return {false, "MILL22B.INF entry jump contradicts loaded native memory"};
        }
    }
    auto next = checkpoint_;
    next.last_sequence = observation.sequence;
    next.state = MillenniumAtariPostConfigEntryState::status_register_boundary;
    next.entry_jump_executed = true;
    next.entry_jump_observed = true;
    next.status_register_address = checkpoint_.entry_jump_target;
    checkpoint_ = std::move(next);
    return {true, {}};
}

MillenniumAtariPostConfigEntryResult
MillenniumAtariPostConfigEntrySession::execute_entry_jump(
    const NativeRuntimeMemory& memory, const std::uint64_t transfer_sequence,
    const std::uint32_t transfer_return_address) {
    if (checkpoint_.state != MillenniumAtariPostConfigEntryState::entry_jump_boundary) {
        return {false, "MILL22B.INF entry jump is unavailable"};
    }
    if (transfer_sequence == 0 || transfer_sequence <= checkpoint_.last_sequence) {
        return {false, "MILL22B.INF entry jump has a stale transfer sequence"};
    }
    if (transfer_return_address != checkpoint_.entry_jump_address) {
        return {false, "MILL22B.INF loader RTS did not return to its entry buffer"};
    }
    for (std::size_t index = 0; index < 6; ++index) {
        const auto byte = memory.read_byte({NativeRuntimeAddressSpace::linear, std::nullopt,
            static_cast<std::uint64_t>(checkpoint_.entry_jump_address) + index});
        if (!byte || *byte != module_[index]) {
            return {false, "MILL22B.INF entry jump contradicts loaded native memory"};
        }
    }

    auto next = checkpoint_;
    next.last_sequence = transfer_sequence;
    next.state = MillenniumAtariPostConfigEntryState::status_register_boundary;
    next.entry_jump_executed = true;
    next.status_register_address = checkpoint_.entry_jump_target;
    checkpoint_ = std::move(next);
    return {true, {}};
}

MillenniumAtariPostConfigEntryResult
MillenniumAtariPostConfigEntrySession::observe_status_register(
    const MillenniumAtariStatusRegisterObservation& observation,
    const NativeRuntimeMemory& memory) {
    if (checkpoint_.state != MillenniumAtariPostConfigEntryState::status_register_boundary) {
        return {false, "MILL22B.INF status-register boundary is unavailable"};
    }
    if (observation.generation != checkpoint_.generation
        || observation.sequence <= checkpoint_.last_sequence
        || observation.instruction_address != checkpoint_.status_register_address) {
        return {false, "MILL22B.INF status-register observation is stale or at the wrong address"};
    }
    constexpr std::uint16_t supervisor_mask = 0x2000;
    constexpr std::uint16_t zero_mask = 0x0004;
    const bool supervisor = (observation.status_register & supervisor_mask) != 0;
    if (supervisor != (observation.privilege == MillenniumAtariObservedPrivilege::supervisor)) {
        return {false, "MILL22B.INF status value contradicts observed privilege"};
    }
    const auto matches_loaded_module = [&](const std::uint32_t runtime_address,
                                            const std::size_t module_offset,
                                            const std::size_t byte_count) {
        if (module_offset > module_.size() || byte_count > module_.size() - module_offset) {
            return false;
        }
        for (std::size_t index = 0; index < byte_count; ++index) {
            const auto byte = memory.read_byte({NativeRuntimeAddressSpace::linear, std::nullopt,
                static_cast<std::uint64_t>(runtime_address) + index});
            if (!byte || *byte != module_[module_offset + index]) return false;
        }
        return true;
    };
    constexpr std::size_t entry_prologue_offset = 0xa82c;
    constexpr std::size_t entry_prologue_bytes = 24;
    constexpr std::uint32_t helper_address = 0x1f924;
    constexpr std::size_t helper_file_offset = helper_address - 0x11e00;
    constexpr std::size_t helper_through_trap_bytes = 12;
    if (!matches_loaded_module(0x1c62c, entry_prologue_offset, entry_prologue_bytes)
        || !matches_loaded_module(helper_address, helper_file_offset, helper_through_trap_bytes)) {
        return {false, "MILL22B.INF entry/helper bytes are missing or contradict loaded native memory"};
    }

    auto next = checkpoint_;
    next.last_sequence = observation.sequence;
    next.state = MillenniumAtariPostConfigEntryState::xbios_trap_boundary;
    next.status_register_observed = true;
    next.observed_status_register = observation.status_register;
    next.observed_privilege = observation.privilege;
    next.supervisor_branch_taken = supervisor;
    next.resulting_data_register = supervisor
        ? 0x07ffU : static_cast<std::uint16_t>(observation.status_register & ~supervisor_mask);
    // BCLR #13,D0 sets only CCR.Z according to the cleared bit. Supervisor
    // execution then reaches MOVE #$0300,SR before the local XBIOS helper.
    next.resulting_status_register = supervisor
        ? 0x0300U
        : static_cast<std::uint16_t>((observation.status_register & ~zero_mask) | zero_mask);
    next.local_jsr_return_address = 0x1c64e;
    next.xbios_trap_address = 0x1f92e;
    next.xbios_selector = 0x0026;
    next.xbios_pointer_argument = 0x1f934;
    next.relative_stack_delta = -6;
    next.relative_stack_bytes = {0x00, 0x26, 0x00, 0x01, 0xf9, 0x34};
    if (supervisor) {
        next.hardware_writes = {
            {1, 0x1c63c, 0xffff8800U, 0x07},
            {2, 0x1c63c, 0xffff8802U, 0xff},
            {3, 0x1c640, 0xffff8800U, 0x0e},
        };
    }
    checkpoint_ = std::move(next);
    return {true, {}};
}

MillenniumAtariPostConfigEntryResult
MillenniumAtariPostConfigEntrySession::observe_xbios_26_return(
    const MillenniumAtariPostConfigXbiosReturnObservation& observation) {
    if (checkpoint_.state != MillenniumAtariPostConfigEntryState::xbios_trap_boundary) {
        return {false, "XBIOS selector-$26 return is unavailable"};
    }
    if (observation.generation != checkpoint_.generation
        || observation.sequence <= checkpoint_.last_sequence
        || observation.trap_address != checkpoint_.xbios_trap_address
        || observation.selector != checkpoint_.xbios_selector) {
        return {false, "XBIOS selector-$26 return observation is stale or mismatched"};
    }
    // EON_ARTIFACT_POLICY_ALLOW: XBIOS selector-$26 ABI argument frame.
    constexpr std::array<std::uint8_t, 6> expected_arguments{
        0x00, 0x26, 0x00, 0x01, 0xf9, 0x34};
    if (observation.argument_bytes != expected_arguments
        || observation.post_service_pc != 0x1f930
        || observation.argument_stack_address > UINT32_MAX - 10U
        || observation.post_service_address[7] != observation.argument_stack_address
        || observation.rts_stack_address != observation.argument_stack_address + 6U
        || observation.rts_stack_address < 2U
        || observation.rts_stack_address > UINT32_MAX - 4U) {
        return {false, "XBIOS selector-$26 runtime frame contradicts the verified call"};
    }
    const auto return_address = (static_cast<std::uint32_t>(observation.rts_stack_bytes[0]) << 24U)
        | (static_cast<std::uint32_t>(observation.rts_stack_bytes[1]) << 16U)
        | (static_cast<std::uint32_t>(observation.rts_stack_bytes[2]) << 8U)
        | observation.rts_stack_bytes[3];
    if (return_address != checkpoint_.local_jsr_return_address) {
        return {false, "XBIOS selector-$26 RTS frame does not return to $1c64e"};
    }

    auto next = checkpoint_;
    next.last_sequence = observation.sequence;
    next.xbios_26_return_observed = true;
    next.xbios_26_trap_address = observation.trap_address;
    next.xbios_26_selector = observation.selector;
    next.xbios_26_argument_stack_address = observation.argument_stack_address;
    next.xbios_26_argument_bytes = observation.argument_bytes;
    next.xbios_26_post_service_pc = observation.post_service_pc;
    next.xbios_26_post_service_sr = observation.post_service_sr;
    next.xbios_26_post_service_data = observation.post_service_data;
    next.xbios_26_post_service_address = observation.post_service_address;
    next.xbios_26_rts_stack_address = observation.rts_stack_address;
    next.xbios_26_rts_return_address = return_address;
    next.xbios_26_memory_effects = observation.memory_effects;
    next.local_jsr_return_address_materialized = true;
    next.selector_15_trap_address = 0x1c654;
    next.selector_15_argument_stack_address = observation.rts_stack_address - 2U;
    next.selector_15_argument_bytes = {0x00, 0x15, 0x00, 0x00, 0x00, 0x00};
    next.xbios_trap_address = next.selector_15_trap_address;
    next.xbios_selector = 0x0015;
    next.xbios_pointer_argument = 0;
    next.relative_stack_delta = -2;
    next.relative_stack_bytes.assign(next.selector_15_argument_bytes.begin(),
        next.selector_15_argument_bytes.end());
    next.state = MillenniumAtariPostConfigEntryState::xbios_selector_15_boundary;
    checkpoint_ = std::move(next);
    return {true, {}};
}

MillenniumAtariPostConfigEntryResult
MillenniumAtariPostConfigEntrySession::observe_xbios_result(
    const MillenniumAtariPostConfigXbiosResultObservation& observation,
    const NativeRuntimeMemory& memory) {
    if (checkpoint_.state != MillenniumAtariPostConfigEntryState::xbios_selector_15_boundary
        && checkpoint_.state != MillenniumAtariPostConfigEntryState::xbios_selector_2_boundary
        && checkpoint_.state != MillenniumAtariPostConfigEntryState::xbios_selector_3_boundary
        && checkpoint_.state != MillenniumAtariPostConfigEntryState::xbios_selector_4_boundary) {
        return {false, "Post-config XBIOS result is unavailable"};
    }
    if (observation.generation != checkpoint_.generation
        || observation.sequence <= checkpoint_.last_sequence) {
        return {false, "Post-config XBIOS result is stale"};
    }

    constexpr std::size_t caller_offset = 0xa854;
    constexpr std::size_t caller_size = 10;
    constexpr std::uint32_t caller_address = 0x1c654;
    constexpr std::size_t local_prefix_offset = 0x18;
    constexpr std::size_t local_prefix_size = 44;
    constexpr std::uint32_t local_prefix_address = 0x11e18;
    const auto loaded_span_matches = [&](const std::uint32_t address,
        const std::size_t module_offset, const std::size_t size) {
        if (module_offset > module_.size() || size > module_.size() - module_offset) return false;
        for (std::size_t index = 0; index < size; ++index) {
            const auto byte = memory.read_byte({NativeRuntimeAddressSpace::linear, std::nullopt,
                static_cast<std::uint64_t>(address) + index});
            if (!byte || *byte != module_[module_offset + index]) return false;
        }
        return true;
    };
    const auto loaded_longword = [&](const std::uint32_t address,
        std::uint32_t& value) {
        std::array<std::uint8_t, 4> bytes{};
        for (std::size_t index = 0; index < bytes.size(); ++index) {
            const auto byte = memory.read_byte({NativeRuntimeAddressSpace::linear, std::nullopt,
                static_cast<std::uint64_t>(address) + index});
            if (!byte) return false;
            bytes[index] = *byte;
        }
        value = (static_cast<std::uint32_t>(bytes[0]) << 24U)
            | (static_cast<std::uint32_t>(bytes[1]) << 16U)
            | (static_cast<std::uint32_t>(bytes[2]) << 8U)
            | bytes[3];
        return true;
    };
    if (!loaded_span_matches(caller_address, caller_offset, caller_size)
        || !loaded_span_matches(local_prefix_address, local_prefix_offset, local_prefix_size)) {
        return {false, "Post-config caller or selector continuation is missing or changed in native memory"};
    }

    std::uint32_t expected_trap = 0;
    std::uint16_t expected_selector = 0;
    std::uint32_t expected_pc = 0;
    std::uint32_t expected_stack = 0;
    std::vector<std::uint8_t> expected_arguments;
    std::uint32_t write_address = 0;
    std::uint8_t write_width = 0;

    switch (checkpoint_.state) {
    case MillenniumAtariPostConfigEntryState::xbios_selector_15_boundary:
        expected_trap = 0x1c654;
        expected_selector = 0x0015;
        expected_pc = 0x1c656;
        expected_stack = checkpoint_.selector_15_argument_stack_address;
        expected_arguments.assign(checkpoint_.selector_15_argument_bytes.begin(),
            checkpoint_.selector_15_argument_bytes.end());
        if (expected_stack > UINT32_MAX - 8U) {
            return {false, "Selector-$15 argument stack wraps the address space"};
        }
        break;
    case MillenniumAtariPostConfigEntryState::xbios_selector_2_boundary:
        expected_trap = 0x11e1c;
        expected_selector = 0x0002;
        expected_pc = 0x11e1e;
        expected_stack = checkpoint_.post_config_result_argument_stack_address;
        if (checkpoint_.post_config_call_stack_address < 2U
            || expected_stack != checkpoint_.post_config_call_stack_address - 2U) {
            return {false, "Selector-$2 stack does not follow the observed local JSR"};
        }
        expected_arguments = {0x00, 0x02};
        write_address = 0x11e06;
        write_width = 4;
        break;
    case MillenniumAtariPostConfigEntryState::xbios_selector_3_boundary:
        expected_trap = 0x11e2a;
        expected_selector = 0x0003;
        expected_pc = 0x11e2c;
        expected_stack = checkpoint_.post_config_result_argument_stack_address;
        if (checkpoint_.post_config_call_stack_address < 2U
            || expected_stack != checkpoint_.post_config_call_stack_address - 2U) {
            return {false, "Selector-$3 stack does not follow the observed local JSR"};
        }
        expected_arguments = {0x00, 0x03};
        write_address = 0x11e0a;
        write_width = 4;
        {
        std::uint32_t stored = 0;
        if (!loaded_longword(0x11e06, stored)
            || stored != checkpoint_.post_config_raw_result_d0[0]) {
            return {false, "Selector-$2 raw D0 store is absent from native memory"};
        }
        }
        break;
    case MillenniumAtariPostConfigEntryState::xbios_selector_4_boundary:
        expected_trap = 0x11e38;
        expected_selector = 0x0004;
        expected_pc = 0x11e3a;
        expected_stack = checkpoint_.post_config_result_argument_stack_address;
        if (checkpoint_.post_config_call_stack_address < 2U
            || expected_stack != checkpoint_.post_config_call_stack_address - 2U) {
            return {false, "Selector-$4 stack does not follow the observed local JSR"};
        }
        expected_arguments = {0x00, 0x04};
        write_address = 0x11e0e;
        write_width = 2;
        {
        std::uint32_t stored_2 = 0;
        std::uint32_t stored_3 = 0;
        if (!loaded_longword(0x11e06, stored_2)
            || stored_2 != checkpoint_.post_config_raw_result_d0[0]
            || !loaded_longword(0x11e0a, stored_3)
            || stored_3 != checkpoint_.post_config_raw_result_d0[1]) {
            return {false, "Selector-$2/$3 raw D0 stores are absent from native memory"};
        }
        }
        break;
    default:
        return {false, "Post-config XBIOS result is unavailable"};
    }

    if (observation.trap_address != expected_trap || observation.selector != expected_selector
        || observation.argument_stack_address != expected_stack
        || observation.argument_bytes != expected_arguments
        || observation.post_service_pc != expected_pc
        || observation.post_service_a7 != expected_stack) {
        return {false, "Post-config XBIOS result contradicts the verified trap frame"};
    }

    auto next = checkpoint_;
    next.last_sequence = observation.sequence;
    next.post_config_result_argument_stack_address = expected_stack;
    next.post_config_result_trap_address = expected_trap;
    next.post_config_result_selector = expected_selector;
    next.post_config_result_argument_bytes = expected_arguments;
    next.post_config_result_write_address = write_address;
    next.post_config_result_write_width = write_width;
    next.post_config_result_write_value = observation.result_d0;
    switch (checkpoint_.state) {
    case MillenniumAtariPostConfigEntryState::xbios_selector_15_boundary:
        next.selector_15_return_observed = true;
        next.selector_15_result_d0 = observation.result_d0;
        // The caller's ADDQ.L #6,A7 and JSR $11e18 are in the verified ten
        // bytes above. JSR pushes return PC $1c65e, then the local routine
        // pushes selector words at this same two-byte frame.
        next.post_config_call_stack_address = expected_stack + 2U;
        next.post_config_result_argument_stack_address = expected_stack;
        next.post_config_result_trap_address = 0x11e1c;
        next.post_config_result_selector = 0x0002;
        next.post_config_result_argument_bytes = {0x00, 0x02};
        next.xbios_trap_address = 0x11e1c;
        next.xbios_selector = 0x0002;
        next.xbios_pointer_argument = 0;
        next.relative_stack_delta = -2;
        next.relative_stack_bytes = {0x00, 0x02};
        next.state = MillenniumAtariPostConfigEntryState::xbios_selector_2_boundary;
        break;
    case MillenniumAtariPostConfigEntryState::xbios_selector_2_boundary:
        next.post_config_raw_result_d0[0] = observation.result_d0;
        next.post_config_result_trap_address = 0x11e2a;
        next.post_config_result_selector = 0x0003;
        next.post_config_result_argument_bytes = {0x00, 0x03};
        next.post_config_result_write_address = write_address;
        next.post_config_result_write_width = write_width;
        next.xbios_trap_address = 0x11e2a;
        next.xbios_selector = 0x0003;
        next.xbios_pointer_argument = 0;
        next.relative_stack_delta = -2;
        next.relative_stack_bytes = {0x00, 0x03};
        next.state = MillenniumAtariPostConfigEntryState::xbios_selector_3_boundary;
        break;
    case MillenniumAtariPostConfigEntryState::xbios_selector_3_boundary:
        next.post_config_raw_result_d0[1] = observation.result_d0;
        next.post_config_result_trap_address = 0x11e38;
        next.post_config_result_selector = 0x0004;
        next.post_config_result_argument_bytes = {0x00, 0x04};
        next.post_config_result_write_address = write_address;
        next.post_config_result_write_width = write_width;
        next.xbios_trap_address = 0x11e38;
        next.xbios_selector = 0x0004;
        next.xbios_pointer_argument = 0;
        next.relative_stack_delta = -2;
        next.relative_stack_bytes = {0x00, 0x04};
        next.state = MillenniumAtariPostConfigEntryState::xbios_selector_4_boundary;
        break;
    case MillenniumAtariPostConfigEntryState::xbios_selector_4_boundary:
        next.post_config_raw_result_d0[2] = observation.result_d0;
        next.post_config_result_write_value = observation.result_d0 & 0xffffU;
        next.line_a_instruction_address = 0x11e42;
        next.line_a_instruction_opcode = 0xa000;
        next.post_config_result_trap_address = 0;
        next.post_config_result_selector = 0;
        next.post_config_result_argument_bytes.clear();
        next.xbios_trap_address = 0;
        next.xbios_selector = 0;
        next.xbios_pointer_argument = 0;
        next.relative_stack_delta = 0;
        next.relative_stack_bytes.clear();
        next.state = MillenniumAtariPostConfigEntryState::line_a_instruction_boundary;
        break;
    default:
        return {false, "Post-config XBIOS result is unavailable"};
    }
    checkpoint_ = std::move(next);
    return {true, {}};
}

NativeRuntimeEffectBatch MillenniumAtariPostConfigEntrySession::make_hardware_effect_batch(
    std::string id) const {
    if (checkpoint_.state != MillenniumAtariPostConfigEntryState::xbios_trap_boundary
        || !checkpoint_.status_register_observed || id.empty()) {
        throw std::runtime_error("MILL22B.INF hardware writes are unavailable");
    }
    NativeRuntimeEffectBatch batch{std::move(id), true, {}};
    batch.effects.reserve(checkpoint_.hardware_writes.size());
    for (const auto& write : checkpoint_.hardware_writes) {
        batch.effects.push_back({write.order,
            {NativeRuntimeAddressSpace::linear, std::nullopt, write.address},
            MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian,
            write.value});
    }
    return batch;
}

NativeRuntimeEffectBatch MillenniumAtariPostConfigEntrySession::make_xbios_result_effect_batch(
    std::string id) const {
    if (checkpoint_.post_config_result_write_width == 0 || id.empty()) {
        return {std::move(id), true, {}};
    }
    if (checkpoint_.post_config_result_write_width != 2
        && checkpoint_.post_config_result_write_width != 4) {
        throw std::runtime_error("Unsupported post-config XBIOS result width");
    }
    NativeRuntimeEffectBatch batch{std::move(id), true, {}};
    batch.effects.push_back({1,
        {NativeRuntimeAddressSpace::linear, std::nullopt,
            checkpoint_.post_config_result_write_address},
        checkpoint_.post_config_result_write_width == 4
            ? MemoryTransferElementWidth::longword : MemoryTransferElementWidth::word,
        NativeRuntimeByteOrder::big_endian,
        checkpoint_.post_config_result_write_value});
    return batch;
}

MillenniumAtariPostConfigEntryResult
MillenniumAtariPostConfigEntrySession::observe_line_a_return(
    const MillenniumAtariPostConfigLineAObservation& observation,
    const NativeRuntimeMemory& memory) {
    if (checkpoint_.state != MillenniumAtariPostConfigEntryState::line_a_instruction_boundary) {
        return {false, "MILL22B.INF Line-A boundary is unavailable"};
    }
    if (observation.generation != checkpoint_.generation
        || observation.sequence <= checkpoint_.last_sequence
        || observation.instruction_address != checkpoint_.line_a_instruction_address
        || observation.instruction_opcode != checkpoint_.line_a_instruction_opcode
        || observation.instruction_address != 0x11e42U
        || observation.instruction_opcode != 0xa000U
        || observation.post_service_pc != 0x11e44U) {
        return {false, "MILL22B.INF Line-A return observation is stale or mismatched"};
    }

    constexpr std::uint32_t local_suffix_address = 0x11e42U;
    constexpr std::size_t local_suffix_file_offset = 0x42U;
    constexpr std::size_t local_suffix_byte_count = 24U;
    constexpr std::uint32_t caller_address = 0x1c65eU;
    constexpr std::size_t caller_file_offset = 0xa85eU;
    constexpr std::size_t caller_byte_count = 0x60U;
    constexpr std::uint32_t local_callee_address = 0x1c5a0U;
    constexpr std::size_t local_callee_file_offset = 0xa7a0U;
    constexpr std::size_t local_callee_byte_count = 14U;
    const auto loaded_span_matches = [&](const std::uint32_t address,
        const std::size_t module_offset, const std::size_t size) {
        if (module_offset > module_.size() || size > module_.size() - module_offset) return false;
        for (std::size_t index = 0; index < size; ++index) {
            const auto byte = memory.read_byte({NativeRuntimeAddressSpace::linear, std::nullopt,
                static_cast<std::uint64_t>(address) + index});
            if (!byte || *byte != module_[module_offset + index]) return false;
        }
        return true;
    };
    if (!loaded_span_matches(local_suffix_address, local_suffix_file_offset,
            local_suffix_byte_count)
        || !loaded_span_matches(caller_address, caller_file_offset, caller_byte_count)
        || !loaded_span_matches(local_callee_address, local_callee_file_offset,
            local_callee_byte_count)) {
        return {false, "MILL22B.INF Line-A suffix or caller code is absent or changed in native memory"};
    }

    const auto a0 = observation.post_service_address[0];
    if (a0 > UINT32_MAX - 16U || observation.post_service_address[7] > UINT32_MAX - 4U
        || observation.rts_stack_address != observation.post_service_address[7]) {
        return {false, "MILL22B.INF Line-A register image has a wrapping A0/A7 operand"};
    }
    const std::array<std::uint32_t, 2> expected_read_addresses{a0 + 8U, a0 + 12U};
    for (std::size_t read_index = 0; read_index < observation.memory_reads.size(); ++read_index) {
        const auto& read = observation.memory_reads[read_index];
        if (read.address != expected_read_addresses[read_index]) {
            return {false, "MILL22B.INF Line-A memory reads do not match the local MOVEA operands"};
        }
        for (std::size_t byte_index = 0; byte_index < read.bytes.size(); ++byte_index) {
            const auto known = memory.read_byte({NativeRuntimeAddressSpace::linear, std::nullopt,
                static_cast<std::uint64_t>(read.address) + byte_index});
            if (known && *known != read.bytes[byte_index]) {
                return {false, "MILL22B.INF Line-A memory read contradicts native memory"};
            }
        }
    }
    const auto read_long = [](const std::array<std::uint8_t, 4>& bytes) {
        return (static_cast<std::uint32_t>(bytes[0]) << 24U)
            | (static_cast<std::uint32_t>(bytes[1]) << 16U)
            | (static_cast<std::uint32_t>(bytes[2]) << 8U)
            | bytes[3];
    };
    const auto local_return = (static_cast<std::uint32_t>(observation.rts_stack_bytes[0]) << 24U)
        | (static_cast<std::uint32_t>(observation.rts_stack_bytes[1]) << 16U)
        | (static_cast<std::uint32_t>(observation.rts_stack_bytes[2]) << 8U)
        | observation.rts_stack_bytes[3];
    if (local_return != 0x1c65eU) {
        return {false, "MILL22B.INF Line-A suffix RTS does not return to caller $1c65e"};
    }

    const auto read_longword = [&](const std::uint32_t address, std::uint32_t& value) {
        std::array<std::uint8_t, 4> bytes{};
        for (std::size_t index = 0; index < bytes.size(); ++index) {
            const auto byte = memory.read_byte({NativeRuntimeAddressSpace::linear, std::nullopt,
                static_cast<std::uint64_t>(address) + index});
            if (!byte) return false;
            bytes[index] = *byte;
        }
        value = read_long(bytes);
        return true;
    };
    std::uint32_t selector_three_word = 0;
    std::uint32_t post_config_fclose_value = 0;
    if (!read_longword(0x11e0aU, selector_three_word)
        || selector_three_word != checkpoint_.post_config_raw_result_d0[1]
        || !read_longword(0x11dfcU, post_config_fclose_value)) {
        return {false, "MILL22B.INF caller data inputs are absent or contradict prior observations"};
    }

    constexpr std::uint32_t caller_expected_d6 = 0x361436a7U;
    const bool compare_equal = post_config_fclose_value == caller_expected_d6;
    auto next = checkpoint_;
    next.last_sequence = observation.sequence;
    next.line_a_return_observed = true;
    next.line_a_post_service_pc = observation.post_service_pc;
    next.line_a_post_service_sr = observation.post_service_sr;
    next.line_a_post_service_data = observation.post_service_data;
    next.line_a_post_service_address = observation.post_service_address;
    next.line_a_memory_reads = observation.memory_reads;
    next.line_a_rts_stack_address = observation.rts_stack_address;
    next.line_a_rts_return_address = local_return;
    next.line_a_result_a3 = read_long(observation.memory_reads[0].bytes);
    next.line_a_result_a4 = read_long(observation.memory_reads[1].bytes);
    next.line_a_local_return_pc = 0x1c65eU;
    next.caller_compare_d6 = post_config_fclose_value;
    next.caller_compare_equal = compare_equal;

    if (!compare_equal) {
        next.next_local_call_boundary_address = 0x1c676U;
        next.state = MillenniumAtariPostConfigEntryState::caller_transfer_boundary;
    } else {
        next.caller_final_d1 = 0x2d4a6U;
        next.caller_stack_reset_address = 0x1cadaU;
        next.first_local_jsr_address = local_callee_address;
        next.first_local_jsr_return_address = 0x1c6beU;
        next.first_local_jsr_stack_address = 0x1cad6U;
        next.next_local_call_boundary_address = 0x1c6beU;
        next.state = MillenniumAtariPostConfigEntryState::post_line_a_subroutine_boundary;
    }
    checkpoint_ = std::move(next);
    return {true, {}};
}

MillenniumAtariPostConfigEntryResult
MillenniumAtariPostConfigEntrySession::observe_local_call_2340c(
    const MillenniumAtariPostConfigLocalCallObservation& observation,
    const NativeRuntimeMemory& memory) {
    if (checkpoint_.state != MillenniumAtariPostConfigEntryState::post_line_a_subroutine_boundary
        || !checkpoint_.line_a_return_observed || !checkpoint_.caller_compare_equal
        || checkpoint_.next_local_call_boundary_address != 0x1c6beU) {
        return {false, "MILL22B.INF local call $2340c is unavailable"};
    }
    if (observation.generation != checkpoint_.generation
        || observation.sequence <= checkpoint_.last_sequence
        || observation.instruction_address != 0x2340cU
        || observation.address_registers[7] != 0x1cad6U
        || observation.rts_stack_address != 0x1cad6U) {
        return {false, "MILL22B.INF local call $2340c entry trace is stale or mismatched"};
    }
    const auto read_long = [](const std::array<std::uint8_t, 4>& bytes) {
        return (static_cast<std::uint32_t>(bytes[0]) << 24U)
            | (static_cast<std::uint32_t>(bytes[1]) << 16U)
            | (static_cast<std::uint32_t>(bytes[2]) << 8U)
            | bytes[3];
    };
    const auto stack_return = read_long(observation.rts_stack_bytes);
    if (stack_return != 0x1c6c4U
        || observation.data_registers[0] != 0x1cae0U
        || observation.data_registers[1] != checkpoint_.caller_final_d1) {
        return {false, "MILL22B.INF local call $2340c input registers or return frame disagree"};
    }
    for (std::size_t index = 2; index < 6; ++index) {
        if (observation.data_registers[index] != checkpoint_.line_a_post_service_data[index]) {
            return {false, "MILL22B.INF local call $2340c preserved data registers disagree"};
        }
    }
    if (observation.data_registers[6] != checkpoint_.caller_compare_d6
        || observation.data_registers[7] != checkpoint_.line_a_post_service_data[7]) {
        return {false, "MILL22B.INF local call $2340c D6/D7 inputs disagree"};
    }
    for (std::size_t index = 0; index < 7; ++index) {
        if (observation.address_registers[index] != checkpoint_.line_a_post_service_address[index]) {
            return {false, "MILL22B.INF local call $2340c preserved address registers disagree"};
        }
    }

    constexpr std::array<std::pair<std::uint32_t, std::pair<std::size_t, std::size_t>>, 7>
        code_spans{{
            {0x2340cU, {0x1160cU, 0x16U}},
            {0x14e20U, {0x3020U, 0x28U}},
            {0x2010eU, {0xe30eU, 0x18U}},
            {0x1fffcU, {0xe1fcU, 0x33U}},
            {0x2009eU, {0xe29eU, 0x3cU}},
            {0x11eb8U, {0xb8U, 0x06U}},
            {0x1decfU, {0xc0cfU, 0x04U}},
        }};
    for (const auto& [address, span] : code_spans) {
        const auto [module_offset, byte_count] = span;
        if (module_offset > module_.size() || byte_count > module_.size() - module_offset) {
            return {false, "MILL22B.INF local call proof range is outside the module"};
        }
        for (std::size_t index = 0; index < byte_count; ++index) {
            const auto loaded = memory.read_byte({NativeRuntimeAddressSpace::linear, std::nullopt,
                static_cast<std::uint64_t>(address) + index});
            if (!loaded || *loaded != module_[module_offset + index]) {
                return {false, "MILL22B.INF local call code or bytecode is absent or changed"};
            }
        }
    }

    const auto read_longword = [&](const std::uint32_t address, std::uint32_t& value) {
        std::array<std::uint8_t, 4> bytes{};
        for (std::size_t index = 0; index < bytes.size(); ++index) {
            const auto byte = memory.read_byte({NativeRuntimeAddressSpace::linear, std::nullopt,
                static_cast<std::uint64_t>(address) + index});
            if (!byte) return false;
            bytes[index] = *byte;
        }
        value = read_long(bytes);
        return true;
    };
    std::uint32_t previous_return = 0;
    std::uint32_t copy_destination = 0;
    std::uint32_t vector_base = 0;
    std::uint32_t selector_three = 0;
    if (!read_longword(0x1cad6U, previous_return)
        || previous_return != 0x1c6beU
        || !read_longword(0x12428U, copy_destination)
        || copy_destination != 0x27326U
        || !read_longword(0x1242cU, vector_base)
        || vector_base != 0x1cae0U
        || !read_longword(0x11e0aU, selector_three)
        || selector_three != checkpoint_.post_config_raw_result_d0[1]) {
        return {false, "MILL22B.INF local call input memory is absent or disagrees with earlier stores"};
    }

    constexpr std::size_t copy_byte_count = 0x17c0U * 4U;
    constexpr std::uint32_t source_delta = 0x1680U;
    if (selector_three > UINT32_MAX - source_delta
        || observation.copy_source_address != selector_three + source_delta
        || observation.copy_source_bytes.size() != copy_byte_count
        || observation.copy_source_address > UINT32_MAX - copy_byte_count
        || copy_destination > UINT32_MAX - copy_byte_count
        || (observation.copy_source_address & 1U) != 0
        || (copy_destination & 1U) != 0) {
        return {false, "MILL22B.INF local call copy observation has an invalid source or size"};
    }
    const auto source_end = static_cast<std::uint64_t>(observation.copy_source_address)
        + copy_byte_count;
    const auto destination_end = static_cast<std::uint64_t>(copy_destination) + copy_byte_count;
    if (observation.copy_source_address < destination_end
        && copy_destination < source_end) {
        return {false, "MILL22B.INF local call copy ranges overlap and are not admitted"};
    }
    for (std::size_t index = 0; index < observation.copy_source_bytes.size(); ++index) {
        const auto known = memory.read_byte({NativeRuntimeAddressSpace::linear, std::nullopt,
            static_cast<std::uint64_t>(observation.copy_source_address) + index});
        if (known && *known != observation.copy_source_bytes[index]) {
            return {false, "MILL22B.INF local call source bytes disagree with native memory"};
        }
    }

    constexpr std::uint32_t table_word_address = 0x1cd36U;
    std::array<std::uint8_t, 2> table_word_bytes{};
    for (std::size_t index = 0; index < table_word_bytes.size(); ++index) {
        const auto byte = memory.read_byte({NativeRuntimeAddressSpace::linear, std::nullopt,
            table_word_address + index});
        if (!byte || *byte != module_[0xaf36U + index]) {
            return {false, "MILL22B.INF local call bytecode table entry is absent or changed"};
        }
        table_word_bytes[index] = *byte;
    }
    const auto table_offset = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(table_word_bytes[0]) << 8U) | table_word_bytes[1]);
    if (table_offset != 0x13efU || vector_base + table_offset != 0x1decfU
        || module_[0xc0cfU] != 0x16U || module_[0xc0d0U] != 0x01U
        || module_[0xc0d1U] != 0x06U || module_[0xc0d2U] != 0x10U) {
        return {false, "MILL22B.INF local call does not resolve to the verified first command"};
    }

    // The command-$16 handler consumes the first two bytes after its opcode.
    // Record the exact next token at $1ded2; the separate continuation below
    // executes the remaining handlers, renderer, and external trap boundary.
    constexpr std::uint16_t first_operand = 0x01U;
    constexpr std::uint16_t second_operand = 0x06U;
    std::uint32_t d0 = second_operand;
    if (d0 >= 0x19U) d0 = 0x18U;
    d0 = static_cast<std::uint16_t>(d0 << 3U);
    d0 *= 0xa0U;
    std::uint16_t d1 = first_operand;
    const bool shifted_carry = (d1 & 1U) != 0;
    d1 = static_cast<std::uint16_t>(d1 >> 1U);
    if (shifted_carry) d0 = static_cast<std::uint16_t>(d0 + 1U);
    d1 = static_cast<std::uint16_t>(d1 << 3U);
    d0 = static_cast<std::uint16_t>(d0 + d1);
    const auto macro_store_value = (selector_three & 0xffff0000U)
        | static_cast<std::uint16_t>(static_cast<std::uint16_t>(selector_three) + d0);

    auto next = checkpoint_;
    next.state = MillenniumAtariPostConfigEntryState::bytecode_command_boundary;
    next.last_sequence = observation.sequence;
    next.local_call_observed = true;
    next.local_call_entry_address = observation.instruction_address;
    next.local_call_entry_return_address = 0x1c6c4U;
    next.local_call_entry_data = observation.data_registers;
    next.local_call_entry_address_registers = observation.address_registers;
    next.local_call_copy_source_address = observation.copy_source_address;
    next.local_call_copy_destination_address = copy_destination;
    next.local_call_copy_byte_count = copy_byte_count;
    next.local_call_copy_source_bytes = observation.copy_source_bytes;
    next.local_call_macro_address = 0x1decfU;
    next.local_call_macro_next_address = 0x1ded2U;
    next.local_call_macro_store_value = macro_store_value;
    next.bytecode_next_address = 0x1ded2U;
    next.bytecode_dispatch_return_address = 0x20026U;
    next.local_call_trap_address = 0;
    next.local_call_trap_selector = 0;
    next.local_call_trap_stack_address = 0;
    next.local_call_return_address = 0;
    next.next_local_call_boundary_address = 0x1ded2U;
    checkpoint_ = std::move(next);
    return {true, {}};
}

MillenniumAtariPostConfigEntryResult
MillenniumAtariPostConfigEntrySession::execute_bytecode_continuation(
    const NativeRuntimeMemory& memory) {
    if (checkpoint_.state != MillenniumAtariPostConfigEntryState::bytecode_command_boundary
        || !checkpoint_.local_call_observed
        || checkpoint_.bytecode_next_address != 0x1ded2U) {
        return {false, "MILL22B.INF bytecode continuation is unavailable"};
    }

    const auto loaded_span_matches = [&](const std::uint32_t address,
                                         const std::size_t size,
                                         const char* expected_hash) {
        if (address < 0x11e00U) return false;
        const auto offset = static_cast<std::size_t>(address - 0x11e00U);
        if (offset > module_.size() || size > module_.size() - offset) return false;
        const auto span = std::span<const std::uint8_t>(module_).subspan(offset, size);
        if (to_hex(sha256(span)) != expected_hash) return false;
        for (std::size_t index = 0; index < size; ++index) {
            const auto byte = memory.read_byte({NativeRuntimeAddressSpace::linear,
                std::nullopt, static_cast<std::uint64_t>(address) + index});
            if (!byte || *byte != span[index]) return false;
        }
        return true;
    };
    if (!loaded_span_matches(0x1decfU, 0x2cU,
            "c4c430dfed566d9f150ff3bc759f97d9642d6b819db86a00d638cd44cfc2f037")
        || !loaded_span_matches(0x2000cU, 0x11cU,
            "6ecf4af4b37b7e5d4bee56a475cc1d12cc765239d8858cdcf92188876ac00ac9")
        || !loaded_span_matches(0x2009eU, 0x3cU,
            "1c40ffe44b9d4365635b1e802e7472bc32f45dcd0cbe64f474b7361074c71e33")
        || !loaded_span_matches(0x1ffa0U, 0x28U,
            "2fcb317f626e51083cc64349aa163c086c887339e6acc0b3977c9a50fa3e60d9")
        || !loaded_span_matches(0x1ffcaU, 0x28U,
            "163b347c6a3fe378ece8ee2382581ff88a6f28f682dbb10df84e7f15860b781c")
        || !loaded_span_matches(0x20128U, 0x128U,
            "7b9320e4f8536869521d87f6c51b05834d5b6450856715d16d15dce0c9b0a990")
        || !loaded_span_matches(0x201e0U, 0x70U,
            "b17d55dd892c07ea84a53cd1c6ba2a07e212ed2f6cfa9f3632363c6cdaabf970")
        || !loaded_span_matches(0x2340cU, 0x16U,
            "e773ba06fbe710796e42e0b323b3ed2b63a9d97d6b087e1f8386e060c1096be2")
        || !loaded_span_matches(0x2010eU, 0x18U,
            "02d0c1902077eccefdd88b85e64823bd1a004f7375afa357ac679074c8af3377")
        || !loaded_span_matches(0x11eb8U, 8U,
            "e4d0962ff2756c0df3139a525645b0c1997a25fa2edae517f55ff70123d6122e")) {
        return {false, "MILL22B.INF macro, renderer, or trap code is absent or changed"};
    }

    const auto read_word = [&](const std::uint32_t address,
                               std::uint16_t& value) {
        const auto high = memory.read_byte({NativeRuntimeAddressSpace::linear,
            std::nullopt, address});
        const auto low = memory.read_byte({NativeRuntimeAddressSpace::linear,
            std::nullopt, static_cast<std::uint64_t>(address) + 1U});
        if (!high || !low) return false;
        value = static_cast<std::uint16_t>((static_cast<std::uint16_t>(*high) << 8U) | *low);
        return true;
    };
    const auto read_long = [&](const std::uint32_t address,
                               std::uint32_t& value) {
        std::uint16_t high = 0;
        std::uint16_t low = 0;
        if (!read_word(address, high)
            || !read_word(address + 2U, low)) return false;
        value = (static_cast<std::uint32_t>(high) << 16U) | low;
        return true;
    };

    std::uint32_t selector_three = 0;
    std::uint32_t stored_screen_pointer = 0;
    std::uint16_t selector_four = 0;
    const auto mode_flag = memory.read_byte({NativeRuntimeAddressSpace::linear,
        std::nullopt, 0x1ff76U});
    if (!read_long(0x11e0aU, selector_three)
        || selector_three != checkpoint_.post_config_raw_result_d0[1]
        || !read_word(0x11e0eU, selector_four)
        || selector_four != static_cast<std::uint16_t>(
            checkpoint_.post_config_raw_result_d0[2])
        || !read_long(0x1ff66U, stored_screen_pointer)
        || stored_screen_pointer != checkpoint_.local_call_macro_store_value
        || !mode_flag || *mode_flag != 1U
        || checkpoint_.local_call_copy_source_bytes.size() != 0x5f00U) {
        return {false, "MILL22B.INF macro lacks its observed selector or screen inputs"};
    }

    constexpr std::uint32_t macro_runtime_address = 0x1decfU;
    constexpr std::size_t macro_file_offset = 0xc0cfU;
    constexpr std::size_t macro_size = 44U;
    if (macro_file_offset + macro_size > module_.size()) {
        return {false, "MILL22B.INF macro stream differs from the exact command sequence"};
    }
    for (std::size_t index = 0; index < macro_size; ++index) {
        const auto byte = memory.read_byte({NativeRuntimeAddressSpace::linear,
            std::nullopt, macro_runtime_address + index});
        if (!byte || *byte != module_[macro_file_offset + index]) {
            return {false, "MILL22B.INF macro bytes are absent from loaded memory"};
        }
    }

    const auto command_screen_pointer = [&](const std::uint8_t y,
                                             const std::uint8_t x,
                                             std::uint32_t& pointer) {
        std::uint32_t d0 = x;
        if (d0 >= 0x19U) d0 = 0x18U;
        d0 = static_cast<std::uint16_t>(d0 << 3U);
        d0 *= 0xa0U;
        std::uint16_t d1 = y;
        const bool shifted_carry = (d1 & 1U) != 0;
        d1 = static_cast<std::uint16_t>(d1 >> 1U);
        if (shifted_carry) d0 = static_cast<std::uint16_t>(d0 + 1U);
        d1 = static_cast<std::uint16_t>(d1 << 3U);
        d0 = static_cast<std::uint16_t>(d0 + d1);
        pointer = (selector_three & 0xffff0000U)
            | static_cast<std::uint16_t>(static_cast<std::uint16_t>(selector_three) + d0);
        return true;
    };

    std::uint32_t mask_index_10 = 0x0aU;
    std::uint32_t mask_index_11 = 0U;
    if (selector_four == 2U) {
        mask_index_10 = 1U;
        mask_index_11 = 0U;
    }
    const auto mask_pointer = 0x2380cU + ((mask_index_10 & 0x0fU) << 3U);
    const auto secondary_mask_pointer = 0x2380cU + ((mask_index_11 & 0x0fU) << 3U);

    std::map<std::uint32_t, std::uint8_t> screen_changes;
    const auto source_begin = checkpoint_.local_call_copy_source_address;
    const auto source_end = static_cast<std::uint64_t>(source_begin)
        + checkpoint_.local_call_copy_source_bytes.size();
    const auto read_screen_byte = [&](const std::uint32_t address,
                                      std::uint8_t& value) {
        if (address < source_begin || static_cast<std::uint64_t>(address) >= source_end) {
            return false;
        }
        const auto index = static_cast<std::size_t>(address - source_begin);
        const auto known = memory.read_byte({NativeRuntimeAddressSpace::linear,
            std::nullopt, address});
        if (known && *known != checkpoint_.local_call_copy_source_bytes[index]) return false;
        const auto changed = screen_changes.find(address);
        value = changed == screen_changes.end()
            ? checkpoint_.local_call_copy_source_bytes[index] : changed->second;
        return true;
    };
    const auto draw_text = [&](const std::string_view text,
                               std::uint32_t& screen_pointer) {
        for (const auto character : text) {
            const auto code = static_cast<std::uint8_t>(character);
            if (code < 0x20U || code > 0x7eU) return false;
            const auto glyph_offset = 0xde86U
                + static_cast<std::size_t>(code - 0x20U) * 8U;
            if (glyph_offset > module_.size() || 8U > module_.size() - glyph_offset) {
                return false;
            }
            for (std::size_t row = 0; row < 8U; ++row) {
                const auto glyph = module_[glyph_offset + row];
                const auto inverse_glyph = static_cast<std::uint8_t>(~glyph);
                for (std::size_t plane = 0; plane < 4U; ++plane) {
                    std::uint16_t mask_word = 0;
                    const auto mask_address = static_cast<std::uint64_t>(mask_pointer)
                        + plane * 2U;
                    if (mask_address > UINT32_MAX - 1U
                        || !read_word(static_cast<std::uint32_t>(mask_address), mask_word)) {
                        return false;
                    }
                    const auto row_offset = row * 0xa0U + plane * 2U;
                    if (screen_pointer > UINT32_MAX - row_offset) return false;
                    const auto address = screen_pointer
                        + static_cast<std::uint32_t>(row_offset);
                    std::uint8_t prior = 0;
                    if (!read_screen_byte(address, prior)) return false;
                    const auto mask = static_cast<std::uint8_t>(mask_word);
                    const auto result = static_cast<std::uint8_t>(
                        (prior & inverse_glyph) | (mask & glyph));
                    screen_changes.insert_or_assign(address, result);
                }
            }
            const auto advance = (screen_pointer & 1U) != 0 ? 7U : 1U;
            if (screen_pointer > UINT32_MAX - advance) return false;
            screen_pointer += advance;
        }
        return true;
    };

    std::uint32_t first_screen_pointer = 0;
    if (!command_screen_pointer(1U, 6U, first_screen_pointer)
        || first_screen_pointer != checkpoint_.local_call_macro_store_value) {
        return {false, "MILL22B.INF first screen command disagrees with its admitted prefix"};
    }
    std::uint32_t screen_pointer = first_screen_pointer;
    if (!draw_text("Insert Disk 2 Then", screen_pointer)) {
        return {false, "MILL22B.INF first text raster lacks bounded source bytes"};
    }
    if (!command_screen_pointer(1U, 7U, screen_pointer)) {
        return {false, "MILL22B.INF second screen command wraps its selector-3 base"};
    }
    if (!draw_text("Press Any Key..", screen_pointer)) {
        return {false, "MILL22B.INF second text raster lacks bounded source bytes"};
    }

    auto next = checkpoint_;
    next.state = MillenniumAtariPostConfigEntryState::local_call_trap_boundary;
    next.bytecode_next_address = 0x1defbU;
    next.bytecode_dispatch_return_address = 0x20124U;
    next.bytecode_final_screen_pointer = screen_pointer;
    next.bytecode_mask_pointer = mask_pointer;
    next.bytecode_secondary_mask_pointer = secondary_mask_pointer;
    next.bytecode_continuation_effects.clear();
    next.bytecode_continuation_effects.reserve(screen_changes.size());
    for (const auto& [address, value] : screen_changes) {
        std::uint8_t before = 0;
        if (!read_screen_byte(address, before)) {
            return {false, "MILL22B.INF raster result escaped its observed screen source"};
        }
        // Store the source byte as the before-image even when this address was
        // already changed by an earlier glyph in this same local transaction.
        const auto source_index = static_cast<std::size_t>(address - source_begin);
        before = checkpoint_.local_call_copy_source_bytes[source_index];
        next.bytecode_continuation_effects.push_back({address, before, value});
    }
    next.local_call_trap_address = 0x11ebcU;
    next.local_call_trap_selector = 7U;
    next.local_call_trap_stack_address = 0x1cad0U;
    next.local_call_return_address = 0x23422U;
    next.next_local_call_boundary_address = 0x11ebcU;
    checkpoint_ = std::move(next);
    return {true, {}};
}

MillenniumAtariPostConfigEntryResult
MillenniumAtariPostConfigEntrySession::observe_crawcin_flag_branch(
    const MillenniumAtariPostConfigCrawcinBranchObservation& observation,
    const NativeRuntimeMemory& memory) {
    if (checkpoint_.state != MillenniumAtariPostConfigEntryState::local_call_trap_boundary
        || !checkpoint_.local_call_observed
        || checkpoint_.local_call_trap_address != 0x11ebcU
        || checkpoint_.local_call_trap_selector != 7U) {
        return {false, "MILL22B.INF Crawcin flag branch is unavailable"};
    }
    if (observation.generation != checkpoint_.generation
        || observation.sequence <= checkpoint_.last_sequence
        || observation.instruction_address != 0x1fc6eU
        || observation.flag_address != 0x1fa0fU
        || observation.observed_next_pc
            != (observation.flag_value == 0 ? 0x1fc84U : 0x1fc76U)) {
        return {false, "MILL22B.INF Crawcin flag branch observation is stale or inconsistent"};
    }

    const auto loaded_span_matches = [&](const std::uint32_t address,
                                         const std::size_t count,
                                         const char* expected_hash) {
        if (address < 0x11e00U) return false;
        const auto offset = static_cast<std::size_t>(address - 0x11e00U);
        if (offset > module_.size() || count > module_.size() - offset) return false;
        const auto span = std::span<const std::uint8_t>(module_).subspan(offset, count);
        if (to_hex(sha256(span)) != expected_hash) return false;
        for (std::size_t index = 0; index < count; ++index) {
            const auto byte = memory.read_byte({NativeRuntimeAddressSpace::linear,
                std::nullopt, static_cast<std::uint64_t>(address) + index});
            if (!byte || *byte != span[index]) return false;
        }
        return true;
    };
    // Pin both sides of the local call chain as well as the helper prefix
    // containing the tested flag and conditional branch. The verified span
    // stops immediately before opaque Line-A opcode $a00c at $1fc82.
    if (!loaded_span_matches(0x23422U, 128U,
            "e729e876017d21231785dd9cbfb6b919bbd42e548c6da3d5d1f6e88a9d58876f")
        || !loaded_span_matches(0x14e48U, 128U,
            "2a330a38ccc81c772a6c79dc384b6c017e2b0484ef3cdf75afb937b9a02937d9")
        || !loaded_span_matches(0x1fc10U, 0x72U,
            "45c2b6f36d77764a33fea38d513a18d3b2cb32a52560b248516e27be36946a05")
        || !loaded_span_matches(0x1fc6eU, 8U,
            "5a60f4ddcb2df53fe59220ea036e24031f8393ca951735aeb2983bce78655d58")
        || !loaded_span_matches(0x1fc84U, 2U,
            "1ceeabf0c6a5a30bad12cdac0e3ab015a7188a42e6aebb556aad00bb9cd693ad")) {
        return {false, "MILL22B.INF Crawcin caller/helper bytes are absent or changed"};
    }
    const auto current_flag = memory.read_byte({NativeRuntimeAddressSpace::linear,
        std::nullopt, observation.flag_address});
    if (!current_flag || *current_flag != observation.flag_value) {
        return {false, "MILL22B.INF Crawcin flag observation disagrees with native memory"};
    }

    auto next = checkpoint_;
    next.state = MillenniumAtariPostConfigEntryState::crawcin_flag_branch_observed;
    next.last_sequence = observation.sequence;
    next.crawcin_flag_branch_observed = true;
    next.crawcin_flag_value = observation.flag_value;
    next.crawcin_flag_branch_next_pc = observation.observed_next_pc;
    checkpoint_ = std::move(next);
    return {true, {}};
}

MillenniumAtariPostConfigEntryResult
MillenniumAtariPostConfigEntrySession::observe_gemdos_fopen(
    const MillenniumAtariPostConfigFopenObservation& observation,
    const NativeRuntimeMemory& memory) {
    if (checkpoint_.state != MillenniumAtariPostConfigEntryState::crawcin_flag_branch_observed
        || !checkpoint_.crawcin_flag_branch_observed
        || checkpoint_.crawcin_flag_value != 0
        || checkpoint_.crawcin_flag_branch_next_pc != 0x1fc84U) {
        return {false, "MILL22B.INF zero-flag Fopen boundary is unavailable"};
    }
    // EON_ARTIFACT_POLICY_ALLOW: GEMDOS Fopen ABI argument frame.
    constexpr std::array<std::uint8_t, 8> expected_arguments{
        0x00, 0x3d, 0x00, 0x01, 0x20, 0x4a, 0x00, 0x02};
    if (observation.generation != checkpoint_.generation
        || observation.sequence <= checkpoint_.last_sequence
        || observation.trap_address != 0x11fd8U
        || observation.observed_a7 != observation.argument_stack_address
        || (observation.argument_stack_address & 1U) != 0
        || observation.argument_stack_address > 0x00fffff8U
        || observation.argument_bytes != expected_arguments) {
        return {false, "MILL22B.INF GEMDOS Fopen observation is stale or inconsistent"};
    }

    const auto loaded_span_matches = [&](const std::uint32_t address,
                                         const std::size_t count,
                                         const char* expected_hash) {
        if (address < 0x11e00U) return false;
        const auto offset = static_cast<std::size_t>(address - 0x11e00U);
        if (offset > module_.size() || count > module_.size() - offset) return false;
        const auto span = std::span<const std::uint8_t>(module_).subspan(offset, count);
        if (to_hex(sha256(span)) != expected_hash) return false;
        for (std::size_t index = 0; index < count; ++index) {
            const auto byte = memory.read_byte({NativeRuntimeAddressSpace::linear,
                std::nullopt, static_cast<std::uint64_t>(address) + index});
            if (!byte || *byte != span[index]) return false;
        }
        return true;
    };
    if (!loaded_span_matches(0x11fc8U, 18U,
            "eeb5dbed91cf6866d70615df0c093a325fd1c6660440b7e3c3f96a9e24357b28")
        || !loaded_span_matches(0x23422U, 128U,
            "e729e876017d21231785dd9cbfb6b919bbd42e548c6da3d5d1f6e88a9d58876f")
        || !loaded_span_matches(0x14e20U, 40U,
            "5a59557110f2435a7c795c82d11a2bde496acacf30be05f532024a62f21a9f39")
        || !loaded_span_matches(0x1204aU, 11U,
            "c1e91c254fbd9599cbcc171800552a8f802e0192f1be7d1c6ac7c78acd79c4a3")) {
        return {false, "MILL22B.INF Fopen stub or filename is absent or changed"};
    }
    constexpr std::uint32_t source_delta = 0x1680U;
    constexpr std::uint32_t expected_copy_destination = 0x27326U;
    constexpr std::size_t expected_copy_bytes = 0x5f00U;
    const auto selector_three = checkpoint_.post_config_raw_result_d0[1];
    if (selector_three > UINT32_MAX - source_delta
        || checkpoint_.local_call_copy_source_address != selector_three + source_delta
        || checkpoint_.local_call_copy_destination_address != expected_copy_destination
        || checkpoint_.local_call_copy_byte_count != expected_copy_bytes
        || checkpoint_.local_call_copy_source_bytes.size() != expected_copy_bytes) {
        return {false, "MILL22B.INF Fopen path lacks its admitted reverse-copy source"};
    }
    for (std::size_t index = 0; index < expected_copy_bytes; ++index) {
        const auto expected = checkpoint_.local_call_copy_source_bytes[index];
        const auto source = memory.read_byte({NativeRuntimeAddressSpace::linear,
            std::nullopt, static_cast<std::uint64_t>(checkpoint_.local_call_copy_source_address) + index});
        const auto destination = memory.read_byte({NativeRuntimeAddressSpace::linear,
            std::nullopt, static_cast<std::uint64_t>(expected_copy_destination) + index});
        if (!source || !destination || *source != expected || *destination != expected) {
            return {false, "MILL22B.INF Fopen path reverse-copy bytes disagree with native memory"};
        }
    }
    for (std::size_t index = 0; index < expected_arguments.size(); ++index) {
        const auto byte = memory.read_byte({NativeRuntimeAddressSpace::linear,
            std::nullopt, static_cast<std::uint64_t>(observation.argument_stack_address) + index});
        if (!byte || *byte != expected_arguments[index]) {
            return {false, "MILL22B.INF Fopen arguments disagree with native stack memory"};
        }
    }

    auto next = checkpoint_;
    next.state = MillenniumAtariPostConfigEntryState::gemdos_fopen_boundary;
    next.last_sequence = observation.sequence;
    next.gemdos_fopen_observed = true;
    next.gemdos_fopen_trap_address = observation.trap_address;
    next.gemdos_fopen_argument_stack_address = observation.argument_stack_address;
    next.gemdos_fopen_argument_bytes = observation.argument_bytes;
    checkpoint_ = std::move(next);
    return {true, {}};
}

MillenniumAtariPostConfigEntryResult
MillenniumAtariPostConfigEntrySession::observe_gemdos_fopen_return(
    const MillenniumAtariPostConfigFopenReturnObservation& observation,
    const NativeRuntimeMemory& memory) {
    if (checkpoint_.state != MillenniumAtariPostConfigEntryState::gemdos_fopen_boundary
        || !checkpoint_.gemdos_fopen_observed) {
        return {false, "MILL22B.INF GEMDOS Fopen return is unavailable"};
    }
    const auto stack = checkpoint_.gemdos_fopen_argument_stack_address;
    if (observation.generation != checkpoint_.generation
        || observation.sequence <= checkpoint_.last_sequence
        || observation.trap_address != 0x11fd8U
        || observation.post_service_pc != 0x11fdaU
        || observation.post_service_a7 != stack
        || (stack & 1U) != 0
        || stack > 0x00fffff4U) {
        return {false, "MILL22B.INF GEMDOS Fopen return is stale or inconsistent"};
    }

    // EON_ARTIFACT_POLICY_ALLOW: GEMDOS Fopen ABI argument frame.
    constexpr std::array<std::uint8_t, 8> expected_arguments{
        0x00, 0x3d, 0x00, 0x01, 0x20, 0x4a, 0x00, 0x02};
    for (std::size_t index = 0; index < expected_arguments.size(); ++index) {
        const auto byte = memory.read_byte({NativeRuntimeAddressSpace::linear,
            std::nullopt, static_cast<std::uint64_t>(stack) + index});
        if (!byte || *byte != expected_arguments[index]
            || *byte != checkpoint_.gemdos_fopen_argument_bytes[index]) {
            return {false, "MILL22B.INF GEMDOS arguments changed before service return"};
        }
    }
    std::array<std::uint8_t, 4> return_bytes{};
    for (std::size_t index = 0; index < return_bytes.size(); ++index) {
        const auto byte = memory.read_byte({NativeRuntimeAddressSpace::linear,
            std::nullopt, static_cast<std::uint64_t>(stack) + 8U + index});
        if (!byte) return {false, "MILL22B.INF GEMDOS Fopen RTS address is absent from native stack"};
        return_bytes[index] = *byte;
    }
    const auto return_pc = (static_cast<std::uint32_t>(return_bytes[0]) << 24U)
        | (static_cast<std::uint32_t>(return_bytes[1]) << 16U)
        | (static_cast<std::uint32_t>(return_bytes[2]) << 8U)
        | static_cast<std::uint32_t>(return_bytes[3]);
    if (return_pc == 0 || (return_pc & 1U) != 0 || return_pc > 0x00ffffffU
        || stack > 0x00fffff3U) {
        return {false, "MILL22B.INF GEMDOS Fopen RTS address is invalid"};
    }
    if (observation.post_rts_pc != return_pc
        || observation.post_rts_a7 != stack + 12U) {
        return {false, "MILL22B.INF GEMDOS Fopen RTS does not match observed PC/A7"};
    }

    constexpr std::uint32_t suffix_address = 0x11fdaU;
    constexpr std::size_t suffix_size = 12U;
    constexpr const char* suffix_hash =
        "b8822e86ef570519d8a3ebedfb3164a8e05d3e6305b3f7ca862bdf874439ae59";
    if (suffix_address < 0x11e00U) {
        return {false, "MILL22B.INF GEMDOS Fopen return span is outside the module"};
    }
    const auto suffix_offset = static_cast<std::size_t>(suffix_address - 0x11e00U);
    if (suffix_offset > module_.size() || suffix_size > module_.size() - suffix_offset) {
        return {false, "MILL22B.INF GEMDOS Fopen return span is outside the module"};
    }
    const auto suffix = std::span<const std::uint8_t>(module_).subspan(suffix_offset, suffix_size);
    if (to_hex(sha256(suffix)) != suffix_hash) {
        return {false, "MILL22B.INF GEMDOS Fopen return code hash mismatch"};
    }
    for (std::size_t index = 0; index < suffix_size; ++index) {
        const auto byte = memory.read_byte({NativeRuntimeAddressSpace::linear,
            std::nullopt, static_cast<std::uint64_t>(suffix_address) + index});
        if (!byte || *byte != suffix[index]) {
            return {false, "MILL22B.INF GEMDOS Fopen return code is absent or changed"};
        }
    }

    std::array<std::uint8_t, 12> stack_bytes{};
    for (std::size_t index = 0; index < stack_bytes.size(); ++index) {
        const auto byte = memory.read_byte({NativeRuntimeAddressSpace::linear,
            std::nullopt, static_cast<std::uint64_t>(stack) + index});
        if (!byte) return {false, "MILL22B.INF return suffix stack span is incomplete"};
        stack_bytes[index] = *byte;
    }
    std::array<std::uint8_t, 2> stored_word{};
    for (std::size_t index = 0; index < stored_word.size(); ++index) {
        const auto byte = memory.read_byte({NativeRuntimeAddressSpace::linear,
            std::nullopt, 0x12056U + index});
        if (!byte) return {false, "MILL22B.INF return suffix destination is unmapped"};
        stored_word[index] = *byte;
    }
    std::array<m68k::MemoryRange, 2> execution_memory{{
        {stack, stack_bytes, false},
        {0x12056U, stored_word, true},
    }};
    m68k::MachineState initial;
    initial.pc = suffix_address;
    initial.sr = observation.post_service_sr;
    initial.data[0] = observation.result_d0;
    initial.address[7] = stack;
    const auto execution = m68k::execute(suffix, suffix_address, initial,
        execution_memory, 5U, return_pc);
    if (execution.reason != m68k::StopReason::requested_address
        || execution.instructions_executed != 4U
        || execution.state.pc != observation.post_rts_pc
        || execution.state.address[7] != observation.post_rts_a7) {
        return {false, "MILL22B.INF bounded Fopen suffix execution contradicts observed RTS"};
    }
    auto expected_sr = static_cast<std::uint16_t>(observation.post_service_sr & 0xfff0U);
    if ((observation.result_d0 & 0x80000000U) != 0) expected_sr |= 0x0008U;
    if (observation.result_d0 == 0) expected_sr |= 0x0004U;
    const auto stored_value = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(stored_word[0]) << 8U) | stored_word[1]);
    const auto executed_store = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(execution_memory[1].bytes[0]) << 8U)
        | execution_memory[1].bytes[1]);
    if (execution.state.sr != expected_sr || stored_value != executed_store
        || executed_store != static_cast<std::uint16_t>(observation.result_d0)) {
        return {false, "MILL22B.INF bounded Fopen suffix effects mismatch"};
    }

    constexpr std::uint32_t fcreate_code_address = 0x11fe6U;
    constexpr std::size_t fcreate_code_size = 18U;
    constexpr const char* fcreate_code_hash =
        "352b6ca9a375e016e129667085dbd6fcd89d1e2eece201d75d97972c60b94cd4";
    // EON_ARTIFACT_POLICY_ALLOW: GEMDOS Fcreate ABI argument frame.
    constexpr std::array<std::uint8_t, 8> expected_fcreate_arguments{
        0x00, 0x3c, 0x00, 0x01, 0x20, 0x4a, 0x00, 0x00};
    bool fcreate_boundary_prepared = false;
    std::uint32_t fcreate_trap_address = 0;
    std::uint32_t fcreate_stack_base = 0;
    std::uint32_t fcreate_data_register_7 = 0;
    std::array<std::uint8_t, 8> fcreate_argument_bytes{};
    if (return_pc == fcreate_code_address) {
        const auto fcreate_code_offset = static_cast<std::size_t>(
            fcreate_code_address - 0x11e00U);
        if (fcreate_code_offset > module_.size()
            || fcreate_code_size > module_.size() - fcreate_code_offset) {
            return {false, "MILL22B.INF Fcreate setup span is outside the module"};
        }
        const auto fcreate_code = std::span<const std::uint8_t>(module_).subspan(
            fcreate_code_offset, fcreate_code_size);
        if (to_hex(sha256(fcreate_code)) != fcreate_code_hash) {
            return {false, "MILL22B.INF Fcreate setup code hash mismatch"};
        }
        for (std::size_t index = 0; index < fcreate_code.size(); ++index) {
            const auto byte = memory.read_byte({NativeRuntimeAddressSpace::linear,
                std::nullopt, static_cast<std::uint64_t>(fcreate_code_address) + index});
            if (!byte || *byte != fcreate_code[index]) {
                return {false, "MILL22B.INF Fcreate setup code is absent or changed"};
            }
        }
        if (execution.state.address[7] < 8U) {
            return {false, "MILL22B.INF Fcreate argument stack address underflows"};
        }
        fcreate_stack_base = execution.state.address[7] - 8U;
        if ((fcreate_stack_base & 1U) != 0U) {
            return {false, "MILL22B.INF Fcreate argument stack address is invalid"};
        }
        std::array<std::uint8_t, 8> fcreate_stack{};
        for (std::size_t index = 0; index < fcreate_stack.size(); ++index) {
            const auto byte = memory.read_byte({NativeRuntimeAddressSpace::linear,
                std::nullopt, static_cast<std::uint64_t>(fcreate_stack_base) + index});
            if (!byte) return {false, "MILL22B.INF Fcreate argument stack is incomplete"};
            fcreate_stack[index] = *byte;
        }
        std::array<m68k::MemoryRange, 1> fcreate_memory{{
            {fcreate_stack_base, fcreate_stack, true},
        }};
        const auto fcreate_execution = m68k::execute(fcreate_code, fcreate_code_address,
            execution.state, fcreate_memory, 5U, 0x11fff8U);
        if (fcreate_execution.reason != m68k::StopReason::trap_instruction
            || fcreate_execution.stop_opcode != 0x4e41U
            || fcreate_execution.instructions_executed != 4U
            || fcreate_execution.state.pc != 0x11ff6U
            || fcreate_execution.state.address[7] != fcreate_stack_base
            || fcreate_execution.state.data[7] != 0x1204aU
            || !std::equal(fcreate_memory[0].bytes.begin(), fcreate_memory[0].bytes.end(),
                expected_fcreate_arguments.begin(), expected_fcreate_arguments.end())) {
            return {false, "MILL22B.INF bounded Fcreate setup does not reach the observed trap frame"};
        }
        fcreate_trap_address = fcreate_execution.state.pc;
        fcreate_data_register_7 = fcreate_execution.state.data[7];
        std::copy(fcreate_memory[0].bytes.begin(), fcreate_memory[0].bytes.end(),
            fcreate_argument_bytes.begin());
        fcreate_boundary_prepared = true;
    }

    auto next = checkpoint_;
    next.state = MillenniumAtariPostConfigEntryState::gemdos_fopen_result_observed;
    next.last_sequence = observation.sequence;
    next.gemdos_fopen_result_observed = true;
    next.gemdos_fopen_result_d0 = observation.result_d0;
    next.gemdos_fopen_post_service_sr = observation.post_service_sr;
    next.gemdos_fopen_result_sr = execution.state.sr;
    next.gemdos_fopen_return_pc = return_pc;
    next.gemdos_fopen_result_a7 = stack + 12U;
    next.gemdos_fopen_stored_word = executed_store;
    next.gemdos_fopen_suffix_instruction_count =
        static_cast<std::uint32_t>(execution.instructions_executed);
    next.gemdos_fcreate_boundary_prepared = fcreate_boundary_prepared;
    next.gemdos_fcreate_trap_address = fcreate_trap_address;
    next.gemdos_fcreate_argument_stack_address = fcreate_stack_base;
    next.gemdos_fcreate_argument_bytes = fcreate_argument_bytes;
    next.gemdos_fcreate_stack_base = fcreate_stack_base;
    next.gemdos_fcreate_data_register_7 = fcreate_data_register_7;
    checkpoint_ = std::move(next);
    return {true, {}};
}

MillenniumAtariPostConfigEntryResult
MillenniumAtariPostConfigEntrySession::observe_gemdos_fcreate(
    const MillenniumAtariPostConfigFcreateObservation& observation,
    const NativeRuntimeMemory& memory) {
    if (checkpoint_.state != MillenniumAtariPostConfigEntryState::gemdos_fopen_result_observed
        || !checkpoint_.gemdos_fopen_result_observed
        || !checkpoint_.gemdos_fcreate_boundary_prepared
        || checkpoint_.gemdos_fcreate_observed) {
        return {false, "MILL22B.INF GEMDOS Fcreate boundary is unavailable"};
    }
    // EON_ARTIFACT_POLICY_ALLOW: GEMDOS Fcreate ABI argument frame.
    constexpr std::array<std::uint8_t, 8> expected_arguments{
        0x00, 0x3c, 0x00, 0x01, 0x20, 0x4a, 0x00, 0x00};
    if (observation.generation != checkpoint_.generation
        || observation.sequence <= checkpoint_.last_sequence
        || observation.trap_address != 0x11ff6U
        || observation.trap_address != checkpoint_.gemdos_fcreate_trap_address
        || observation.observed_a7 != observation.argument_stack_address
        || observation.argument_stack_address != checkpoint_.gemdos_fcreate_stack_base
        || observation.argument_bytes != expected_arguments
        || observation.argument_bytes != checkpoint_.gemdos_fcreate_argument_bytes
        || checkpoint_.gemdos_fcreate_data_register_7 != 0x1204aU) {
        return {false, "MILL22B.INF GEMDOS Fcreate observation is stale or inconsistent"};
    }
    constexpr std::size_t code_size = 18U;
    constexpr const char* code_hash =
        "352b6ca9a375e016e129667085dbd6fcd89d1e2eece201d75d97972c60b94cd4";
    const auto code = std::span<const std::uint8_t>(module_).subspan(0x1e6U, code_size);
    if (to_hex(sha256(code)) != code_hash) {
        return {false, "MILL22B.INF GEMDOS Fcreate code hash mismatch"};
    }
    for (std::size_t index = 0; index < code.size(); ++index) {
        const auto byte = memory.read_byte({NativeRuntimeAddressSpace::linear,
            std::nullopt, 0x11fe6U + index});
        if (!byte || *byte != code[index]) {
            return {false, "MILL22B.INF GEMDOS Fcreate code is absent or changed"};
        }
    }
    for (std::size_t index = 0; index < expected_arguments.size(); ++index) {
        const auto byte = memory.read_byte({NativeRuntimeAddressSpace::linear,
            std::nullopt, static_cast<std::uint64_t>(observation.argument_stack_address) + index});
        if (!byte || *byte != expected_arguments[index]) {
            return {false, "MILL22B.INF GEMDOS Fcreate arguments disagree with native stack memory"};
        }
    }
    auto next = checkpoint_;
    next.last_sequence = observation.sequence;
    next.gemdos_fcreate_observed = true;
    checkpoint_ = std::move(next);
    return {true, {}};
}

MillenniumAtariPostConfigEntryResult
MillenniumAtariPostConfigEntrySession::observe_gemdos_fcreate_return(
    const MillenniumAtariPostConfigFcreateReturnObservation& observation,
    const NativeRuntimeMemory& memory) {
    if (checkpoint_.state != MillenniumAtariPostConfigEntryState::gemdos_fopen_result_observed
        || !checkpoint_.gemdos_fcreate_observed || checkpoint_.gemdos_fcreate_result_observed) {
        return {false, "MILL22B.INF GEMDOS Fcreate return is unavailable"};
    }
    const auto stack = checkpoint_.gemdos_fcreate_stack_base;
    if (observation.generation != checkpoint_.generation
        || observation.sequence <= checkpoint_.last_sequence
        || observation.trap_address != 0x11ff6U
        || observation.post_service_pc != 0x11ff8U
        || observation.post_service_a7 != stack
        || (stack & 1U) != 0U || stack > 0x00fffff3U) {
        return {false, "MILL22B.INF GEMDOS Fcreate return is stale or inconsistent"};
    }
    for (std::size_t index = 0; index < checkpoint_.gemdos_fcreate_argument_bytes.size(); ++index) {
        const auto byte = memory.read_byte({NativeRuntimeAddressSpace::linear,
            std::nullopt, static_cast<std::uint64_t>(stack) + index});
        if (!byte || *byte != checkpoint_.gemdos_fcreate_argument_bytes[index]) {
            return {false, "MILL22B.INF Fcreate arguments changed before service return"};
        }
    }
    std::array<std::uint8_t, 12> stack_bytes{};
    for (std::size_t index = 0; index < stack_bytes.size(); ++index) {
        const auto byte = memory.read_byte({NativeRuntimeAddressSpace::linear,
            std::nullopt, static_cast<std::uint64_t>(stack) + index});
        if (!byte) return {false, "MILL22B.INF Fcreate RTS address is absent from native stack"};
        stack_bytes[index] = *byte;
    }
    const auto return_pc = (static_cast<std::uint32_t>(stack_bytes[8]) << 24U)
        | (static_cast<std::uint32_t>(stack_bytes[9]) << 16U)
        | (static_cast<std::uint32_t>(stack_bytes[10]) << 8U)
        | static_cast<std::uint32_t>(stack_bytes[11]);
    if (return_pc == 0U || (return_pc & 1U) != 0U || return_pc > 0x00ffffffU
        || stack > 0x00fffff1U || observation.post_rts_a7 != stack + 12U
        || observation.post_rts_pc != return_pc) {
        return {false, "MILL22B.INF Fcreate RTS does not match observed PC/A7"};
    }

    constexpr std::uint32_t suffix_address = 0x11ff8U;
    constexpr std::size_t suffix_size = 12U;
    constexpr const char* suffix_hash =
        "b8822e86ef570519d8a3ebedfb3164a8e05d3e6305b3f7ca862bdf874439ae59";
    constexpr std::size_t suffix_offset = suffix_address - 0x11e00U;
    if (suffix_offset > module_.size() || suffix_size > module_.size() - suffix_offset) {
        return {false, "MILL22B.INF Fcreate return span is outside the module"};
    }
    const auto suffix = std::span<const std::uint8_t>(module_).subspan(
        suffix_offset, suffix_size);
    if (to_hex(sha256(suffix)) != suffix_hash) {
        return {false, "MILL22B.INF Fcreate return code hash mismatch"};
    }
    for (std::size_t index = 0; index < suffix.size(); ++index) {
        const auto byte = memory.read_byte({NativeRuntimeAddressSpace::linear,
            std::nullopt, static_cast<std::uint64_t>(suffix_address) + index});
        if (!byte || *byte != suffix[index]) {
            return {false, "MILL22B.INF Fcreate return code is absent or changed"};
        }
    }
    std::array<std::uint8_t, 2> stored_word{};
    for (std::size_t index = 0; index < stored_word.size(); ++index) {
        const auto byte = memory.read_byte({NativeRuntimeAddressSpace::linear,
            std::nullopt, 0x12056U + index});
        if (!byte) return {false, "MILL22B.INF Fcreate result destination is unmapped"};
        stored_word[index] = *byte;
    }
    std::array<m68k::MemoryRange, 2> execution_memory{{
        {stack, stack_bytes, false},
        {0x12056U, stored_word, true},
    }};
    m68k::MachineState initial;
    initial.pc = suffix_address;
    initial.sr = observation.post_service_sr;
    initial.data[0] = observation.result_d0;
    initial.address[7] = stack;
    const auto execution = m68k::execute(suffix, suffix_address, initial,
        execution_memory, 5U, return_pc);
    const auto expected_sr = [&] {
        auto value = static_cast<std::uint16_t>(observation.post_service_sr & 0xfff0U);
        if ((observation.result_d0 & 0x80000000U) != 0U) value |= 0x0008U;
        if (observation.result_d0 == 0U) value |= 0x0004U;
        return value;
    }();
    const auto executed_word = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(execution_memory[1].bytes[0]) << 8U)
        | execution_memory[1].bytes[1]);
    if (execution.reason != m68k::StopReason::requested_address
        || execution.instructions_executed != 4U
        || execution.state.pc != observation.post_rts_pc
        || execution.state.address[7] != observation.post_rts_a7
        || execution.state.sr != expected_sr
        || executed_word != static_cast<std::uint16_t>(observation.result_d0)) {
        return {false, "MILL22B.INF bounded Fcreate suffix contradicts observed RTS/result"};
    }
    auto next = checkpoint_;
    next.state = MillenniumAtariPostConfigEntryState::gemdos_fcreate_result_observed;
    next.last_sequence = observation.sequence;
    next.gemdos_fcreate_result_observed = true;
    next.gemdos_fcreate_result_d0 = observation.result_d0;
    next.gemdos_fcreate_post_service_sr = observation.post_service_sr;
    next.gemdos_fcreate_result_sr = execution.state.sr;
    next.gemdos_fcreate_return_pc = return_pc;
    next.gemdos_fcreate_result_a7 = stack + 12U;
    next.gemdos_fcreate_stored_word = executed_word;
    next.gemdos_fcreate_suffix_instruction_count =
        static_cast<std::uint32_t>(execution.instructions_executed);
    checkpoint_ = std::move(next);
    return {true, {}};
}

NativeRuntimeEffectBatch
MillenniumAtariPostConfigEntrySession::make_line_a_continuation_effect_batch(
    std::string id) const {
    if (checkpoint_.state != MillenniumAtariPostConfigEntryState::post_line_a_subroutine_boundary
        && checkpoint_.state != MillenniumAtariPostConfigEntryState::caller_transfer_boundary) {
        throw std::runtime_error("MILL22B.INF Line-A continuation effects are unavailable");
    }
    if (!checkpoint_.line_a_return_observed || id.empty()) {
        throw std::runtime_error("MILL22B.INF Line-A continuation effects are unavailable");
    }
    if ((checkpoint_.state == MillenniumAtariPostConfigEntryState::caller_transfer_boundary
            && checkpoint_.caller_compare_equal)
        || (checkpoint_.state == MillenniumAtariPostConfigEntryState::post_line_a_subroutine_boundary
            && !checkpoint_.caller_compare_equal)) {
        throw std::runtime_error("MILL22B.INF Line-A continuation effects are unavailable");
    }
    NativeRuntimeEffectBatch batch{std::move(id), true, {}};
    const auto append_long = [&](const std::uint32_t address, const std::uint32_t value) {
        batch.effects.push_back({batch.effects.size() + 1U,
            {NativeRuntimeAddressSpace::linear, std::nullopt, address},
            MemoryTransferElementWidth::longword, NativeRuntimeByteOrder::big_endian, value});
    };
    append_long(0x11e10U, checkpoint_.line_a_result_a3);
    append_long(0x11e14U, checkpoint_.line_a_result_a4);
    append_long(0x1ff66U, checkpoint_.line_a_post_service_data[0]);
    if (!checkpoint_.caller_compare_equal) return batch;

    append_long(0x1c58cU, checkpoint_.caller_compare_d6);
    append_long(0x12424U, 0x268d6U);
    append_long(0x12428U, 0x27326U);
    append_long(0x12430U, 0x2d226U);
    append_long(0x12434U, checkpoint_.caller_final_d1);
    append_long(checkpoint_.first_local_jsr_stack_address,
        checkpoint_.first_local_jsr_return_address);
    append_long(0x1242cU, 0x1cae0U);
    return batch;
}

NativeRuntimeEffectBatch
MillenniumAtariPostConfigEntrySession::make_local_call_2340c_effect_batch(std::string id) const {
    if (checkpoint_.state != MillenniumAtariPostConfigEntryState::bytecode_command_boundary
        || !checkpoint_.local_call_observed || id.empty()) {
        throw std::runtime_error("MILL22B.INF local call $2340c effects are unavailable");
    }
    NativeRuntimeEffectBatch batch{std::move(id), true, {}};
    batch.effects.reserve(checkpoint_.local_call_copy_byte_count / 2U + 2U);
    const auto copy_span_size = checkpoint_.local_call_copy_byte_count;
    // The accepted trace supplies this exact source image. Mirror it into
    // owned guest memory, then materialize the verified longword copy.
    if (checkpoint_.local_call_copy_source_bytes.size() != copy_span_size) {
        throw std::runtime_error("MILL22B.INF local call source trace is unavailable");
    }
    for (std::size_t offset = 0; offset < copy_span_size; offset += 4U) {
        const auto& bytes = checkpoint_.local_call_copy_source_bytes;
        const auto value = (static_cast<std::uint32_t>(bytes[offset]) << 24U)
            | (static_cast<std::uint32_t>(bytes[offset + 1U]) << 16U)
            | (static_cast<std::uint32_t>(bytes[offset + 2U]) << 8U)
            | bytes[offset + 3U];
        batch.effects.push_back({batch.effects.size() + 1U,
            {NativeRuntimeAddressSpace::linear, std::nullopt,
                checkpoint_.local_call_copy_source_address + offset},
            MemoryTransferElementWidth::longword, NativeRuntimeByteOrder::big_endian, value});
    }
    for (std::size_t offset = 0; offset < copy_span_size; offset += 4U) {
        const auto& bytes = checkpoint_.local_call_copy_source_bytes;
        const auto value = (static_cast<std::uint32_t>(bytes[offset]) << 24U)
            | (static_cast<std::uint32_t>(bytes[offset + 1U]) << 16U)
            | (static_cast<std::uint32_t>(bytes[offset + 2U]) << 8U)
            | bytes[offset + 3U];
        batch.effects.push_back({batch.effects.size() + 1U,
            {NativeRuntimeAddressSpace::linear, std::nullopt,
                checkpoint_.local_call_copy_destination_address + offset},
            MemoryTransferElementWidth::longword, NativeRuntimeByteOrder::big_endian, value});
    }
    // The observed dispatcher sets its mode flag before command $16 stores
    // the selector-3 address adjusted by operands 1 and 6.
    batch.effects.push_back({batch.effects.size() + 1U,
        {NativeRuntimeAddressSpace::linear, std::nullopt, 0x1ff76U},
        MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian, 1U});
    batch.effects.push_back({batch.effects.size() + 1U,
        {NativeRuntimeAddressSpace::linear, std::nullopt, 0x1ff66U},
        MemoryTransferElementWidth::longword, NativeRuntimeByteOrder::big_endian,
        checkpoint_.local_call_macro_store_value});
    return batch;
}

NativeRuntimeEffectBatch
MillenniumAtariPostConfigEntrySession::make_bytecode_continuation_effect_batch(
    std::string id) const {
    if (checkpoint_.state != MillenniumAtariPostConfigEntryState::local_call_trap_boundary
        || checkpoint_.local_call_trap_address != 0x11ebcU
        || checkpoint_.local_call_trap_selector != 7U
        || checkpoint_.local_call_trap_stack_address != 0x1cad0U
        || checkpoint_.bytecode_continuation_effects.empty() || id.empty()) {
        throw std::runtime_error("MILL22B.INF bytecode continuation effects are unavailable");
    }
    NativeRuntimeEffectBatch batch{std::move(id), true, {}};
    batch.effects.reserve(checkpoint_.bytecode_continuation_effects.size() + 5U);
    const auto append = [&](const std::uint32_t address,
                            const MemoryTransferElementWidth width,
                            const std::uint32_t value) {
        batch.effects.push_back({batch.effects.size() + 1U,
            {NativeRuntimeAddressSpace::linear, std::nullopt, address},
            width, NativeRuntimeByteOrder::big_endian, value});
    };
    for (const auto& effect : checkpoint_.bytecode_continuation_effects) {
        append(effect.address, MemoryTransferElementWidth::byte, effect.after);
    }
    append(0x1ff5eU, MemoryTransferElementWidth::longword,
        checkpoint_.bytecode_mask_pointer);
    append(0x1ff62U, MemoryTransferElementWidth::longword,
        checkpoint_.bytecode_secondary_mask_pointer);
    append(0x1ff66U, MemoryTransferElementWidth::longword,
        checkpoint_.bytecode_final_screen_pointer);
    append(0x1cad2U, MemoryTransferElementWidth::longword,
        checkpoint_.local_call_return_address);
    append(checkpoint_.local_call_trap_stack_address,
        MemoryTransferElementWidth::word, checkpoint_.local_call_trap_selector);
    return batch;
}

NativeRuntimeEffectBatch
MillenniumAtariPostConfigEntrySession::make_gemdos_fopen_result_effect_batch(
    std::string id) const {
    if (checkpoint_.state != MillenniumAtariPostConfigEntryState::gemdos_fopen_result_observed
        || !checkpoint_.gemdos_fopen_result_observed || id.empty()) {
        throw std::runtime_error("MILL22B.INF GEMDOS Fopen result effects are unavailable");
    }
    NativeRuntimeEffectBatch batch{std::move(id), true, {{1,
        {NativeRuntimeAddressSpace::linear, std::nullopt, 0x12056U},
        MemoryTransferElementWidth::word, NativeRuntimeByteOrder::big_endian,
        checkpoint_.gemdos_fopen_stored_word}}};
    if (checkpoint_.gemdos_fcreate_boundary_prepared) {
        for (std::size_t index = 0; index < checkpoint_.gemdos_fcreate_argument_bytes.size(); ++index) {
            batch.effects.push_back({batch.effects.size() + 1U,
                {NativeRuntimeAddressSpace::linear, std::nullopt,
                    static_cast<std::uint64_t>(checkpoint_.gemdos_fcreate_stack_base) + index},
                MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian,
                checkpoint_.gemdos_fcreate_argument_bytes[index]});
        }
    }
    return batch;
}

NativeRuntimeEffectBatch
MillenniumAtariPostConfigEntrySession::make_gemdos_fcreate_result_effect_batch(
    std::string id) const {
    if (checkpoint_.state != MillenniumAtariPostConfigEntryState::gemdos_fcreate_result_observed
        || !checkpoint_.gemdos_fcreate_result_observed || id.empty()) {
        throw std::runtime_error("MILL22B.INF GEMDOS Fcreate result effects are unavailable");
    }
    return {std::move(id), true, {{1,
        {NativeRuntimeAddressSpace::linear, std::nullopt, 0x12056U},
        MemoryTransferElementWidth::word, NativeRuntimeByteOrder::big_endian,
        checkpoint_.gemdos_fcreate_stored_word}}};
}

} // namespace eon
