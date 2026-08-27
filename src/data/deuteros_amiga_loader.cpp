#include "data/deuteros_amiga_loader.hpp"

#include <span>
#include <stdexcept>

namespace eon {
namespace {

std::uint16_t big16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) throw std::runtime_error("Truncated 68000 word");
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8U) | bytes[offset + 1]);
}

std::uint32_t big32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(big16(bytes, offset)) << 16U) | big16(bytes, offset + 2);
}

void require_word(std::span<const std::uint8_t> bytes, std::size_t offset, std::uint16_t opcode) {
    if (big16(bytes, offset) != opcode) throw std::runtime_error("Unexpected Deuteros loader opcode");
}

} // namespace

DeuterosAmigaLoadPlan parse_deuteros_amiga_load_plan(const AmigaAdf& disk) {
    if (disk.kind() != AmigaDiskKind::dos || !disk.boot_checksum_valid()) {
        throw std::runtime_error("Not a verified Deuteros Amiga system disk");
    }
    const auto boot = disk.boot_block();
    // move.l #length,$24(a1); move.l #destination,$28(a1)
    require_word(boot, 0x32, 0x237c);
    require_word(boot, 0x3a, 0x237c);
    // move.l #track,d7; mulu.w #track_size,d7
    require_word(boot, 0x42, 0x2e3c);
    require_word(boot, 0x48, 0xcefc);
    // movea.l #entry,a0
    require_word(boot, 0x82, 0x207c);
    const auto loader_length = big32(boot, 0x34);
    const auto loader_destination = big32(boot, 0x3c);
    const auto loader_track = big32(boot, 0x44);
    const auto track_size = static_cast<std::uint32_t>(big16(boot, 0x4a));
    const auto loader_entry = big32(boot, 0x84);
    if (loader_length != track_size || track_size != AmigaAdf::sector_size * AmigaAdf::sectors_per_track) {
        throw std::runtime_error("Deuteros loader track geometry mismatch");
    }
    const AmigaLoadStage loader{loader_track * track_size, loader_length,
        loader_destination, loader_entry};
    if (loader.entry_address < loader.destination
        || loader.entry_address - loader.destination >= loader.length) {
        throw std::runtime_error("Deuteros bootstrap entry outside loaded track");
    }

    // Boot passes D0=0, selecting the first absolute function pointer at
    // memory $12a36. Translate it back into the loaded ADF track.
    require_word(boot, 0x88, 0x7000); // moveq #0,d0
    constexpr std::uint32_t profile_table_address = 0x12a36;
    const auto table_offset = loader.disk_offset + profile_table_address - loader.destination;
    const auto table = disk.bytes(table_offset, 8);
    const auto parse_profile = [&](std::size_t index) {
        const auto profile_address = big32(table, index * 4);
        if (profile_address < loader.destination
            || profile_address - loader.destination + 20 > loader.length) {
            throw std::runtime_error("Deuteros bootstrap profile outside loader track");
        }
        const auto profile_offset = loader.disk_offset + profile_address - loader.destination;
        const auto profile = disk.bytes(profile_offset, 20);
        // move.l #destination,d1; move.l #length,d0; move.l #track,d2; rts
        require_word(profile, 0, 0x223c);
        require_word(profile, 6, 0x203c);
        require_word(profile, 12, 0x243c);
        require_word(profile, 18, 0x4e75);
        const auto destination = big32(profile, 2);
        const auto length = big32(profile, 8);
        const auto track = big32(profile, 14);
        const auto disk_offset = track * track_size;
        static_cast<void>(disk.bytes(disk_offset, length));
        return DeuterosAmigaBootstrapProfile{disk_offset, length, destination};
    };
    const auto profile_zero = parse_profile(0);
    const auto title_handoff_profile = parse_profile(1);
    const auto stage_bytes = disk.bytes(profile_zero.disk_offset, profile_zero.length);
    require_word(stage_bytes, 0, 0x4ef9); // jmp absolute long
    const auto entry = big32(stage_bytes, 2);
    const AmigaLoadStage main_stage{profile_zero.disk_offset, profile_zero.length,
        profile_zero.destination, entry};

    // The resource loader at $21932 indexes five longwords at $21708. Both
    // addresses reside in the verified main stage, so translate the table
    // back to its ADF position instead of duplicating its contents.
    constexpr std::uint32_t resource_table_address = 0x21708;
    if (resource_table_address < main_stage.destination
        || resource_table_address - main_stage.destination + 20 > main_stage.length) {
        throw std::runtime_error("Deuteros resource table outside main stage");
    }
    const auto resource_table = disk.bytes(main_stage.disk_offset
        + resource_table_address - main_stage.destination, 20);
    std::array<std::uint32_t, 5> resource_offsets{};
    for (std::size_t index = 0; index < resource_offsets.size(); ++index) {
        resource_offsets[index] = big32(resource_table, index * 4);
        static_cast<void>(disk.bytes(resource_offsets[index], 1));
    }
    return {loader, main_stage, resource_offsets, title_handoff_profile};
}

} // namespace eon
