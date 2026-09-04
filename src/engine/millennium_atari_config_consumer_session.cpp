#include "engine/millennium_atari_config_consumer_session.hpp"

#include <array>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace eon {
namespace {

std::uint8_t require_byte(const NativeRuntimeMemory& memory, const std::uint32_t address) {
    const auto value = memory.read_byte(
        {NativeRuntimeAddressSpace::linear, std::nullopt, address});
    if (!value) throw std::runtime_error("Millennium Atari config byte is absent from native memory");
    return *value;
}

} // namespace

MillenniumAtariConfigConsumerSession::MillenniumAtariConfigConsumerSession(
    const std::uint64_t generation, const NativeRuntimeMemory& memory,
    const MillenniumAtariReadOnlyGemdosCheckpoint& gemdos,
    const MillenniumAtariFreadConfigLoadAddressBoundary& load_boundary,
    const MillenniumAtariFreadMappedConfigPrelude& prelude) {
    constexpr std::uint32_t jsr_instruction = 0x7703c;
    constexpr std::uint32_t jsr_return = 0x77042;
    constexpr std::uint32_t jsr_target = 0x2a500;
    constexpr std::uint32_t jump_target = 0x2aa88;
    constexpr std::uint16_t move_sr_d0 = 0x40c0;
    constexpr std::string_view prelude_sha256 =
        "dede20eddbd8015da1d1a4f2f5e53424c2bc2195bff238d830ea24c9f522ea59";
    const std::array<std::uint8_t, 8> required{
        require_byte(memory, jsr_target), require_byte(memory, jsr_target + 1U),
        require_byte(memory, jsr_target + 2U), require_byte(memory, jsr_target + 3U),
        require_byte(memory, jsr_target + 4U), require_byte(memory, jsr_target + 5U),
        require_byte(memory, jump_target), require_byte(memory, jump_target + 1U)};
    constexpr std::array<std::uint8_t, 20> xbios_prefix{
        0x3f, 0x3c, 0x00, 0x02, 0x4e, 0x4e, 0x54, 0x8f,
        0x23, 0xc0, 0x00, 0x02, 0xa5, 0x0a, 0x3f, 0x3c,
        0x00, 0x03, 0x4e, 0x4e,
    };
    for (std::size_t index = 0; index < xbios_prefix.size(); ++index) {
        if (require_byte(memory, 0x2a51cU + static_cast<std::uint32_t>(index))
            != xbios_prefix[index]) {
            throw std::runtime_error("Unexpected Millennium Atari XBIOS continuation bytes");
        }
    }
    constexpr std::array<std::uint8_t, 16> selector_three_continuation{
        0x4e, 0x4e, 0x54, 0x8f, 0x23, 0xc0, 0x00, 0x02,
        0xa5, 0x0e, 0x3f, 0x3c, 0x00, 0x04, 0x4e, 0x4e,
    };
    for (std::size_t index = 0; index < selector_three_continuation.size(); ++index) {
        if (require_byte(memory, 0x2a52eU + static_cast<std::uint32_t>(index))
            != selector_three_continuation[index]) {
            throw std::runtime_error("Unexpected Millennium Atari selector-3 continuation bytes");
        }
    }
    constexpr std::array<std::uint8_t, 12> selector_four_continuation{
        0x4e, 0x4e, 0x54, 0x8f, 0x33, 0xc0,
        0x00, 0x02, 0xa5, 0x12, 0xa0, 0x00,
    };
    for (std::size_t index = 0; index < selector_four_continuation.size(); ++index) {
        if (require_byte(memory, 0x2a53cU + static_cast<std::uint32_t>(index))
            != selector_four_continuation[index]) {
            throw std::runtime_error("Unexpected Millennium Atari selector-4 continuation bytes");
        }
    }
    constexpr std::array<std::uint8_t, 24> line_a_continuation{
        0xa0, 0x00, 0x26, 0x68, 0x00, 0x08, 0x28, 0x68,
        0x00, 0x0c, 0x23, 0xcb, 0x00, 0x02, 0xa5, 0x14,
        0x23, 0xcc, 0x00, 0x02, 0xa5, 0x18, 0x4e, 0x75,
    };
    for (std::size_t index = 0; index < line_a_continuation.size(); ++index) {
        if (require_byte(memory, 0x2a546U + static_cast<std::uint32_t>(index))
            != line_a_continuation[index]) {
            throw std::runtime_error("Unexpected Millennium Atari Line-A continuation bytes");
        }
    }
    constexpr std::array<std::uint8_t, 8> caller_continuation{
        0x42, 0xa7, 0x3f, 0x3c, 0x00, 0x15, 0x4e, 0x4e,
    };
    for (std::size_t index = 0; index < caller_continuation.size(); ++index) {
        if (require_byte(memory, 0x2aaaaU + static_cast<std::uint32_t>(index))
            != caller_continuation[index]) {
            throw std::runtime_error("Unexpected Millennium Atari Line-A caller bytes");
        }
    }
    constexpr std::array<std::uint8_t, 16> selector_21_continuation{
        0x4e, 0x4e, 0x5c, 0x8f, 0x2f, 0x3c, 0x00, 0x02,
        0xa6, 0x12, 0x3f, 0x3c, 0x00, 0x06, 0x4e, 0x4e,
    };
    for (std::size_t index = 0; index < selector_21_continuation.size(); ++index) {
        if (require_byte(memory, 0x2aab0U + static_cast<std::uint32_t>(index))
            != selector_21_continuation[index]) {
            throw std::runtime_error("Unexpected Millennium Atari selector-21 continuation bytes");
        }
    }
    constexpr std::array<std::uint8_t, 10> selector_6_continuation{
        0x4e, 0x4e, 0x5c, 0x8f, 0x4e, 0xb9, 0x00, 0x02, 0xb5, 0x5a,
    };
    for (std::size_t index = 0; index < selector_6_continuation.size(); ++index) {
        if (require_byte(memory, 0x2aabeU + static_cast<std::uint32_t>(index))
            != selector_6_continuation[index]) {
            throw std::runtime_error("Unexpected Millennium Atari selector-6 continuation bytes");
        }
    }
    if (generation == 0 || gemdos.generation != generation
        || gemdos.state != MillenniumAtariReadOnlyGemdosState::config_jsr_boundary
        || gemdos.config_jsr_instruction_address != jsr_instruction
        || gemdos.config_jsr_target_address != jsr_target
        || load_boundary.fread_destination_address != jsr_target
        || load_boundary.payload_initial_jump_opcode != 0x4ef9
        || load_boundary.payload_initial_jump_target_address != jump_target
        || load_boundary.payload_initial_jump_target_file_offset_from_destination != 0x588
        || prelude.fread_destination_address != jsr_target
        || prelude.mapped_entry_address != jump_target
        || prelude.mapped_entry_file_offset != 0x588
        || prelude.initial_opcode != move_sr_d0 || prelude.sha256 != prelude_sha256
        || required != std::array<std::uint8_t, 8>{
            0x4e, 0xf9, 0x00, 0x02, 0xaa, 0x88, 0x40, 0xc0}) {
        throw std::runtime_error("Unexpected Millennium Atari config consumer entry");
    }
    checkpoint_.generation = generation;
    checkpoint_.state = MillenniumAtariConfigConsumerState::status_register_boundary;
    checkpoint_.jsr_instruction_address = jsr_instruction;
    checkpoint_.jsr_return_address = jsr_return;
    checkpoint_.jsr_target_address = jsr_target;
    checkpoint_.entry_jump_opcode = load_boundary.payload_initial_jump_opcode;
    checkpoint_.entry_jump_target_address = jump_target;
    checkpoint_.entry_jump_file_offset =
        load_boundary.payload_initial_jump_target_file_offset_from_destination;
    checkpoint_.boundary_instruction_address = jump_target;
    checkpoint_.boundary_opcode = move_sr_d0;
    checkpoint_.boundary_dependency = "68000 SR privilege/status value";
    checkpoint_.mapped_prelude_sha256 = std::string(prelude_sha256);
    checkpoint_.selector_two_continuation_sha256 =
        "751915c217471e4763ebeef2928dc4cca68bc481dae3113adabb441c2446ee2f";
    checkpoint_.selector_three_continuation_sha256 =
        "f4a7b019591ccff43e4478ac1549e262387ebfb22c16ded18457fe2aca6bbcc2";
    checkpoint_.selector_four_continuation_sha256 =
        "42c6d7ede7609ced9c859e6222d678edf861018b86ee80be2cfe6f8a23010e44";
    checkpoint_.line_a_continuation_sha256 =
        "1705523f57debe7644c3a874cd76e42464f1f34f227c9ee1247026afdb2f3539";
    checkpoint_.line_a_caller_continuation_sha256 =
        "37f9fb95e45dc6c4807821ac79189a2d764fffe6bbbef6196ee17f3ad1a18684";
    checkpoint_.selector_21_continuation_sha256 =
        "de3f0996c3b76c20c1e83a686f9a97f7a5ad8f9575a03d8f01b7f4cadf45a233";
    checkpoint_.selector_6_continuation_sha256 =
        "ba614a28f861921a263225ef85209b20dc2673ea3444cb556b88ca29b2b23163";
    checkpoint_.local_control_transfers_executed = 2;
}

MillenniumAtariConfigConsumerResult
MillenniumAtariConfigConsumerSession::observe_status_register(
    const MillenniumAtariStatusRegisterObservation& observation) {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::status_register_boundary) {
        return {false, "Millennium Atari config consumer is not at the SR boundary"};
    }
    if (observation.generation != checkpoint_.generation || observation.sequence == 0
        || observation.sequence <= checkpoint_.last_sequence
        || observation.instruction_address != checkpoint_.boundary_instruction_address) {
        return {false, "Millennium Atari SR observation is stale or at the wrong instruction"};
    }
    constexpr std::uint16_t supervisor_mask = 0x2000;
    const bool supervisor = (observation.status_register & supervisor_mask) != 0;
    if (supervisor != (observation.privilege == MillenniumAtariObservedPrivilege::supervisor)) {
        return {false, "Millennium Atari SR value contradicts observed privilege"};
    }

    auto next = checkpoint_;
    next.state = MillenniumAtariConfigConsumerState::xbios_trap_boundary;
    next.last_sequence = observation.sequence;
    next.status_register_read = true;
    next.observed_status_register = observation.status_register;
    next.observed_privilege = observation.privilege;
    next.supervisor_bit_was_set = supervisor;
    // BCLR #13,D0 sets Z when the observed S bit was clear. BEQ therefore
    // bypasses hardware setup for an observed user-mode SR.
    next.branch_taken = !supervisor;
    next.converged_jsr_address = 0x2aaa4;
    next.converged_jsr_target = 0x2a51c;
    next.converged_jsr_return_address = 0x2aaaa;
    next.xbios_trap_address = 0x2a520;
    next.xbios_selector = 2;
    next.local_instruction_count = supervisor ? 10U : 5U;
    next.local_control_transfers_executed = supervisor ? 3U : 4U;
    if (supervisor) {
        next.hardware_write_executed = true;
        next.resulting_status_register = 0x0300;
        next.hardware_writes = {
            {1, 0x2aa98, 0xffff8800U, 0x07},
            {2, 0x2aa98, 0xffff8802U, 0xff},
            {3, 0x2aa9c, 0xffff8800U, 0x0e},
        };
    } else {
        // BCLR found a clear bit and therefore sets CCR.Z before BEQ.
        next.resulting_status_register =
            static_cast<std::uint16_t>(observation.status_register | 0x0004U);
    }
    checkpoint_ = std::move(next);
    return {true, {}};
}

std::vector<NativeRuntimeEffectBatch>
MillenniumAtariConfigConsumerSession::make_hardware_effect_batches(
    std::string id_prefix) const {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::xbios_trap_boundary
        || checkpoint_.generation == 0 || id_prefix.empty()) {
        throw std::runtime_error("Millennium Atari hardware effects are not admitted");
    }
    if (!checkpoint_.hardware_write_executed) return {};
    if (checkpoint_.observed_privilege != MillenniumAtariObservedPrivilege::supervisor
        || checkpoint_.hardware_writes.size() != 3) {
        throw std::runtime_error("Millennium Atari hardware effects lack supervisor evidence");
    }
    // MOVEP writes two non-contiguous bytes. MOVE.B then intentionally
    // overwrites the first address, so it is a separate atomic batch.
    NativeRuntimeEffectBatch movep{id_prefix + "-movep", true, {}};
    movep.effects = {
        {1, {NativeRuntimeAddressSpace::linear, std::nullopt, 0xffff8800U},
            MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian, 0x07},
        {2, {NativeRuntimeAddressSpace::linear, std::nullopt, 0xffff8802U},
            MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian, 0xff},
    };
    NativeRuntimeEffectBatch move_byte{id_prefix + "-move-byte", true, {{1,
        {NativeRuntimeAddressSpace::linear, std::nullopt, 0xffff8800U},
        MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian, 0x0e}}};
    return {std::move(movep), std::move(move_byte)};
}

MillenniumAtariConfigConsumerResult
MillenniumAtariConfigConsumerSession::observe_xbios_selector_two(
    const MillenniumAtariXbiosSelectorTwoObservation& observation) {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::xbios_trap_boundary) {
        return {false, "Millennium Atari config consumer is not at XBIOS selector 2"};
    }
    if (observation.generation != checkpoint_.generation
        || observation.sequence <= checkpoint_.last_sequence
        || observation.trap_address != 0x2a520 || observation.selector != 2) {
        return {false, "Millennium Atari XBIOS selector-2 observation is stale or mismatched"};
    }
    auto next = checkpoint_;
    next.state = MillenniumAtariConfigConsumerState::xbios_selector_three_boundary;
    next.last_sequence = observation.sequence;
    next.selector_two_result_observed = true;
    next.selector_two_result_d0 = observation.result_d0;
    next.selector_two_store_address = 0x2a50a;
    next.selector_two_stack_cleanup_bytes = 2;
    next.xbios_trap_address = 0x2a52e;
    next.xbios_selector = 3;
    next.local_instruction_count += 3;
    checkpoint_ = std::move(next);
    return {true, {}};
}

NativeRuntimeEffectBatch
MillenniumAtariConfigConsumerSession::make_selector_two_result_effect_batch(
    std::string id) const {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::xbios_selector_three_boundary
        || !checkpoint_.selector_two_result_observed || checkpoint_.generation == 0
        || checkpoint_.selector_two_store_address != 0x2a50a || id.empty()) {
        throw std::runtime_error("Millennium Atari selector-2 result effect is not admitted");
    }
    return {std::move(id), true, {{1,
        {NativeRuntimeAddressSpace::linear, std::nullopt,
            checkpoint_.selector_two_store_address},
        MemoryTransferElementWidth::longword, NativeRuntimeByteOrder::big_endian,
        checkpoint_.selector_two_result_d0}}};
}

MillenniumAtariConfigConsumerResult
MillenniumAtariConfigConsumerSession::observe_xbios_selector_three(
    const MillenniumAtariXbiosSelectorThreeObservation& observation) {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::xbios_selector_three_boundary) {
        return {false, "Millennium Atari config consumer is not at XBIOS selector 3"};
    }
    if (observation.generation != checkpoint_.generation
        || observation.sequence <= checkpoint_.last_sequence
        || observation.trap_address != 0x2a52e || observation.selector != 3) {
        return {false, "Millennium Atari XBIOS selector-3 observation is stale or mismatched"};
    }
    auto next = checkpoint_;
    next.state = MillenniumAtariConfigConsumerState::xbios_selector_four_boundary;
    next.last_sequence = observation.sequence;
    next.selector_three_result_observed = true;
    next.selector_three_result_d0 = observation.result_d0;
    next.selector_three_store_address = 0x2a50e;
    next.selector_four_stack_cleanup_bytes = 2;
    next.xbios_trap_address = 0x2a53c;
    next.xbios_selector = 4;
    next.local_instruction_count += 3;
    checkpoint_ = std::move(next);
    return {true, {}};
}

NativeRuntimeEffectBatch
MillenniumAtariConfigConsumerSession::make_selector_three_result_effect_batch(
    std::string id) const {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::xbios_selector_four_boundary
        || !checkpoint_.selector_three_result_observed || checkpoint_.generation == 0
        || checkpoint_.selector_three_store_address != 0x2a50e || id.empty()) {
        throw std::runtime_error("Millennium Atari selector-3 result effect is not admitted");
    }
    return {std::move(id), true, {{1,
        {NativeRuntimeAddressSpace::linear, std::nullopt,
            checkpoint_.selector_three_store_address},
        MemoryTransferElementWidth::longword, NativeRuntimeByteOrder::big_endian,
        checkpoint_.selector_three_result_d0}}};
}

MillenniumAtariConfigConsumerResult
MillenniumAtariConfigConsumerSession::observe_xbios_selector_four(
    const MillenniumAtariXbiosSelectorFourObservation& observation) {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::xbios_selector_four_boundary) {
        return {false, "Millennium Atari config consumer is not at XBIOS selector 4"};
    }
    if (observation.generation != checkpoint_.generation
        || observation.sequence <= checkpoint_.last_sequence
        || observation.trap_address != 0x2a53c || observation.selector != 4) {
        return {false, "Millennium Atari XBIOS selector-4 observation is stale or mismatched"};
    }
    auto next = checkpoint_;
    next.state = MillenniumAtariConfigConsumerState::line_a_init_boundary;
    next.last_sequence = observation.sequence;
    next.selector_four_result_observed = true;
    next.selector_four_result_d0_word = static_cast<std::uint16_t>(observation.result_d0);
    next.selector_four_store_address = 0x2a512;
    next.selector_three_stack_cleanup_bytes = 2;
    next.line_a_init_address = 0x2a546;
    next.line_a_init_opcode = 0xa000;
    next.local_instruction_count += 2;
    checkpoint_ = std::move(next);
    return {true, {}};
}

NativeRuntimeEffectBatch
MillenniumAtariConfigConsumerSession::make_selector_four_result_effect_batch(
    std::string id) const {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::line_a_init_boundary
        || !checkpoint_.selector_four_result_observed || checkpoint_.generation == 0
        || checkpoint_.selector_four_store_address != 0x2a512 || id.empty()) {
        throw std::runtime_error("Millennium Atari selector-4 result effect is not admitted");
    }
    return {std::move(id), true, {{1,
        {NativeRuntimeAddressSpace::linear, std::nullopt,
            checkpoint_.selector_four_store_address},
        MemoryTransferElementWidth::word, NativeRuntimeByteOrder::big_endian,
        checkpoint_.selector_four_result_d0_word}}};
}

MillenniumAtariConfigConsumerResult
MillenniumAtariConfigConsumerSession::observe_line_a(
    const MillenniumAtariLineAObservation& observation) {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::line_a_init_boundary) {
        return {false, "Millennium Atari config consumer is not at Line-A init"};
    }
    if (observation.generation != checkpoint_.generation
        || observation.sequence <= checkpoint_.last_sequence
        || observation.instruction_address != 0x2a546
        || observation.returned_a0 > 0xfffffff3U) {
        return {false, "Millennium Atari Line-A observation is stale or mismatched"};
    }
    auto next = checkpoint_;
    next.state = MillenniumAtariConfigConsumerState::xbios_selector_21_boundary;
    next.last_sequence = observation.sequence;
    next.line_a_result_observed = true;
    next.line_a_returned_a0 = observation.returned_a0;
    next.line_a_result_a3 = observation.value_at_a0_plus_8;
    next.line_a_result_a4 = observation.value_at_a0_plus_12;
    next.line_a_a3_store_address = 0x2a514;
    next.line_a_a4_store_address = 0x2a518;
    next.xbios_trap_address = 0x2aab0;
    next.xbios_selector = 0x15;
    next.local_instruction_count += 7;
    checkpoint_ = std::move(next);
    return {true, {}};
}

NativeRuntimeEffectBatch MillenniumAtariConfigConsumerSession::make_line_a_result_effect_batch(
    std::string id) const {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::xbios_selector_21_boundary
        || !checkpoint_.line_a_result_observed || checkpoint_.generation == 0 || id.empty()) {
        throw std::runtime_error("Millennium Atari Line-A result effect is not admitted");
    }
    return {std::move(id), true, {
        {1, {NativeRuntimeAddressSpace::linear, std::nullopt, 0x2a514},
            MemoryTransferElementWidth::longword, NativeRuntimeByteOrder::big_endian,
            checkpoint_.line_a_result_a3},
        {2, {NativeRuntimeAddressSpace::linear, std::nullopt, 0x2a518},
            MemoryTransferElementWidth::longword, NativeRuntimeByteOrder::big_endian,
            checkpoint_.line_a_result_a4},
    }};
}

MillenniumAtariConfigConsumerResult
MillenniumAtariConfigConsumerSession::observe_xbios_selector_21(
    const MillenniumAtariXbiosSelector21Observation& observation) {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::xbios_selector_21_boundary) {
        return {false, "Millennium Atari config consumer is not at XBIOS selector 21"};
    }
    if (observation.generation != checkpoint_.generation
        || observation.sequence <= checkpoint_.last_sequence
        || observation.trap_address != 0x2aab0 || observation.selector != 0x15) {
        return {false, "Millennium Atari XBIOS selector-21 observation is stale or mismatched"};
    }
    auto next = checkpoint_;
    next.state = MillenniumAtariConfigConsumerState::xbios_selector_6_boundary;
    next.last_sequence = observation.sequence;
    next.selector_21_result_observed = true;
    next.selector_21_result_d0 = observation.result_d0;
    next.selector_21_stack_cleanup_bytes = 6;
    next.selector_6_pointer_argument = 0x2a612;
    next.xbios_trap_address = 0x2aabe;
    next.xbios_selector = 6;
    next.local_instruction_count += 3;
    checkpoint_ = std::move(next);
    return {true, {}};
}

MillenniumAtariConfigConsumerResult
MillenniumAtariConfigConsumerSession::observe_xbios_selector_6(
    const MillenniumAtariXbiosSelector6Observation& observation) {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::xbios_selector_6_boundary) {
        return {false, "Millennium Atari config consumer is not at XBIOS selector 6"};
    }
    if (observation.generation != checkpoint_.generation
        || observation.sequence <= checkpoint_.last_sequence
        || observation.trap_address != 0x2aabe || observation.selector != 6) {
        return {false, "Millennium Atari XBIOS selector-6 observation is stale or mismatched"};
    }
    auto next = checkpoint_;
    next.state = MillenniumAtariConfigConsumerState::jsr_2b55a_boundary;
    next.last_sequence = observation.sequence;
    next.selector_6_result_observed = true;
    next.selector_6_result_d0 = observation.result_d0;
    next.selector_6_stack_cleanup_bytes = 6;
    next.next_jsr_address = 0x2aac2;
    next.next_jsr_target = 0x2b55a;
    next.local_instruction_count += 1;
    checkpoint_ = std::move(next);
    return {true, {}};
}

MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_bchg_2b55a(
    const MillenniumAtariBchgObservation& observation) {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::jsr_2b55a_boundary) {
        return {false, "Millennium Atari consumer is not at JSR $2b55a"};
    }
    static_cast<void>(observation);
    return {false, "JSR $2b55a is outside the exact $2a500-loaded config mapping"};
}

NativeRuntimeEffectBatch MillenniumAtariConfigConsumerSession::make_bchg_effect_batch(
    std::string id) const {
    static_cast<void>(id);
    throw std::runtime_error(
        "Millennium Atari JSR $2b55a has no admitted memory effect");
}

MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::revoke(
    const std::uint64_t generation) {
    if (checkpoint_.state == MillenniumAtariConfigConsumerState::revoked) {
        return {false, "Millennium Atari config consumer generation is already revoked"};
    }
    if (generation == 0 || generation != checkpoint_.generation) {
        return {false, "Millennium Atari config consumer revocation generation is stale"};
    }
    checkpoint_.state = MillenniumAtariConfigConsumerState::revoked;
    return {true, {}};
}

} // namespace eon
