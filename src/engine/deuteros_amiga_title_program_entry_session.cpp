#include "engine/deuteros_amiga_title_program_entry_session.hpp"

#include "data/sha256.hpp"

#include <array>

namespace eon {

DeuterosAmigaTitleProgramEntryTransaction
prepare_deuteros_amiga_title_program_entry_transaction(
    const DeuterosAmigaTitleProgramEntrySnapshot& snapshot,
    const NativeRuntimeMemory& memory,
    const DeuterosAmigaTitleStageSession::LocalPrefixAdvance& prefix) {
    DeuterosAmigaTitleProgramEntryTransaction result;
    constexpr std::array<std::uint8_t, 6> expected{{0x4e,0xf9,0x00,0x04,0x04,0x26}};
    if (snapshot.profile != 5 || snapshot.entry_address != 0x13000
        || snapshot.target_address != 0x40426) {
        result.error = "Deuteros title program-entry identity is invalid";
        return result;
    }
    for (std::size_t i = 0; i < expected.size(); ++i) {
        const auto byte = memory.read_byte(
            {NativeRuntimeAddressSpace::linear, std::nullopt, snapshot.entry_address + i});
        if (!byte || *byte != expected[i]) {
            result.error = "Deuteros title program-entry JMP is not owned";
            return result;
        }
    }
    if (snapshot.runtime_memory_checksum != memory.checkpoint().checksum
        || snapshot.jmp_sha256 != to_hex(sha256(expected))) {
        result.error = "Deuteros title program-entry ownership changed";
        return result;
    }
    constexpr std::array<DeuterosAmigaTitleEntryWrite,3> expected_writes{{
        {0x4040e,2,5},{0x3717e,1,5},{0x38092,2,0x0101}}};
    if (prefix.exec_boundary_address != 0x40456
        || prefix.stack_pointer_value != 0x40b62
        || prefix.write_count != expected_writes.size()
        || prefix.writes != expected_writes) {
        result.error = "Deuteros profile-five title re-entry was not admitted";
        return result;
    }

    result.batch = {"deuteros-amiga-title-profile-five-entry", true, {}};
    result.batch.effects.reserve(prefix.write_count + 1);
    result.batch.effects.push_back({1,
        {NativeRuntimeAddressSpace::linear, std::nullopt, 0x206a0},
        MemoryTransferElementWidth::longword, NativeRuntimeByteOrder::big_endian,
        snapshot.controller_pointer});
    for (std::size_t index = 0; index < prefix.write_count; ++index) {
        const auto& write = prefix.writes[index];
        if (write.width_bytes != 1 && write.width_bytes != 2 && write.width_bytes != 4) {
            result.error = "Deuteros title re-entry prefix width is invalid";
            result.batch = {};
            return result;
        }
        result.batch.effects.push_back({result.batch.effects.size() + 1,
            {NativeRuntimeAddressSpace::linear, std::nullopt, write.address},
            static_cast<MemoryTransferElementWidth>(write.width_bytes),
            NativeRuntimeByteOrder::big_endian, write.value});
    }
    result.accepted = true;
    return result;
}

} // namespace eon
