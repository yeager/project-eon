#include "engine/millennium_atari_config_consumer_session.hpp"

#include <array>
#include <stdexcept>
#include <string_view>

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
    checkpoint_ = {generation, MillenniumAtariConfigConsumerState::status_register_boundary,
        jsr_instruction, jsr_return, jsr_target, load_boundary.payload_initial_jump_opcode,
        jump_target, load_boundary.payload_initial_jump_target_file_offset_from_destination,
        jump_target, move_sr_d0, "68000 SR privilege/status value",
        std::string(prelude_sha256), 2, false, false, false};
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
