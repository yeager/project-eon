#include "engine/deuteros_amiga_title_program_entry_session.hpp"

#include "data/sha256.hpp"

#include <algorithm>
#include <vector>

namespace eon {

DeuterosAmigaTitleProgramEntryTransaction
prepare_deuteros_amiga_title_program_entry_transaction(
    const DeuterosAmigaTitleProgramEntrySnapshot& snapshot,
    const NativeRuntimeMemory& memory,
    const DeuterosAmigaTitleStageSession::LocalPrefixAdvance& prefix) {
    DeuterosAmigaTitleProgramEntryTransaction result;
    if ((snapshot.profile != 1 && snapshot.profile != 5) || snapshot.entry_address != 0x13000
        || snapshot.target_address != 0x40426) {
        result.error = "Deuteros title program-entry identity is invalid";
        return result;
    }
    std::vector<std::uint8_t> owned_jmp(6);
    for (std::size_t i = 0; i < owned_jmp.size(); ++i) {
        const auto byte = memory.read_byte(
            {NativeRuntimeAddressSpace::linear, std::nullopt, snapshot.entry_address + i});
        if (!byte) {
            result.error = "Deuteros title program-entry JMP is not owned";
            return result;
        }
        owned_jmp[i] = *byte;
    }
    const auto opcode=static_cast<std::uint16_t>((owned_jmp[0]<<8U)|owned_jmp[1]);
    const auto target=(static_cast<std::uint32_t>(owned_jmp[2])<<24U)
        |(static_cast<std::uint32_t>(owned_jmp[3])<<16U)
        |(static_cast<std::uint32_t>(owned_jmp[4])<<8U)|owned_jmp[5];
    if(opcode!=0x4ef9||target!=snapshot.target_address){
        result.error = "Deuteros title program-entry JMP is not owned";
        return result;
    }
    if (snapshot.runtime_memory_checksum != memory.checkpoint().checksum
        || snapshot.jmp_sha256 != to_hex(sha256(owned_jmp))) {
        result.error = "Deuteros title program-entry ownership changed";
        return result;
    }
    constexpr std::array<DeuterosAmigaTitleEntryWrite,3> profile_five_writes{{
        {0x4040e,2,5},{0x3717e,1,5},{0x38092,2,0x0101}}};
    constexpr std::array<DeuterosAmigaTitleEntryWrite,2> profile_one_writes{{
        {0x4040e,2,1},{0x19d52,1,1}}};
    const auto writes_match = snapshot.profile == 1
        ? prefix.write_count == profile_one_writes.size()
            && std::equal(profile_one_writes.begin(), profile_one_writes.end(), prefix.writes.begin())
        : prefix.write_count == profile_five_writes.size()
            && prefix.writes == profile_five_writes;
    if (prefix.exec_boundary_address != 0x40456
        || prefix.stack_pointer_value != 0x40b62
        || !writes_match) {
        result.error = "Deuteros profile-selected title entry was not admitted";
        return result;
    }

    result.batch = {snapshot.profile==5?"deuteros-amiga-title-profile-five-entry"
        :"deuteros-amiga-title-profile-one-entry", true, {}};
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
