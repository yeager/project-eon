#include "data/atari_st_prg.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace eon {
namespace {

constexpr std::size_t header_bytes = 28;

std::uint16_t read_be16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw std::runtime_error("Truncated Atari ST PRG field");
    }
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8U)
        | bytes[offset + 1]);
}

std::uint32_t read_be32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw std::runtime_error("Truncated Atari ST PRG field");
    }
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U)
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 16U)
        | (static_cast<std::uint32_t>(bytes[offset + 2]) << 8U)
        | bytes[offset + 3];
}

} // namespace

AtariStPrg parse_atari_st_prg(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < header_bytes || read_be16(bytes, 0) != 0x601aU) {
        throw std::runtime_error("Unsupported Atari ST PRG header");
    }

    AtariStPrg result;
    result.text_bytes = read_be32(bytes, 2);
    result.data_bytes = read_be32(bytes, 6);
    result.bss_bytes = read_be32(bytes, 10);
    result.symbol_bytes = read_be32(bytes, 14);
    result.flags = read_be32(bytes, 22);
    result.absolute_flag = read_be16(bytes, 26);

    const auto payload = static_cast<std::uint64_t>(result.text_bytes) + result.data_bytes
        + result.symbol_bytes;
    if (payload > std::numeric_limits<std::size_t>::max()
        || header_bytes > bytes.size()
        || static_cast<std::size_t>(payload) > bytes.size() - header_bytes) {
        throw std::runtime_error("Atari ST PRG segments outside image");
    }
    std::size_t cursor = header_bytes + static_cast<std::size_t>(payload);
    if (result.absolute_flag != 0U) {
        if (cursor != bytes.size()) throw std::runtime_error("Unexpected Atari ST absolute PRG tail");
        return result;
    }
    if (bytes.size() - cursor < 4) throw std::runtime_error("Missing Atari ST PRG relocation table");
    auto relocation = read_be32(bytes, cursor);
    cursor += 4;
    if (relocation == 0U) {
        if (cursor != bytes.size()) throw std::runtime_error("Unexpected Atari ST empty relocation tail");
        return result;
    }
    const auto loadable_bytes = static_cast<std::uint64_t>(result.text_bytes) + result.data_bytes;
    if (loadable_bytes < 4U || relocation > loadable_bytes - 4U) {
        throw std::runtime_error("Atari ST PRG first relocation outside text/data");
    }
    result.first_relocation_offset = relocation;
    result.last_relocation_offset = relocation;
    result.relocation_count = 1;
    const auto loadable_offset = header_bytes;
    result.relocations.push_back({relocation, read_be32(bytes, loadable_offset + relocation)});
    while (cursor < bytes.size()) {
        const auto delta = bytes[cursor++];
        if (delta == 0U) {
            if (cursor != bytes.size()) throw std::runtime_error("Trailing Atari ST PRG relocation bytes");
            return result;
        }
        const auto increment = delta == 1U ? 254U : static_cast<std::uint32_t>(delta);
        if (relocation > std::numeric_limits<std::uint32_t>::max() - increment) {
            throw std::runtime_error("Atari ST PRG relocation overflow");
        }
        relocation += increment;
        if (relocation > loadable_bytes - 4U) {
            throw std::runtime_error("Atari ST PRG relocation outside text/data");
        }
        result.last_relocation_offset = relocation;
        ++result.relocation_count;
        result.relocations.push_back({relocation, read_be32(bytes, loadable_offset + relocation)});
    }
    throw std::runtime_error("Unterminated Atari ST PRG relocation table");
}

MillenniumAtariBootstrap parse_millennium_atari_bootstrap(
    std::span<const std::uint8_t> bytes, const AtariStPrg& prg) {
    // 0: BRA.W $24; $24: LEA source,A0; LEA last-longword,A1;
    // LEA BSS destination,A2; MOVE.L (A0)+,(A2)+; CMPA.L A0,A1;
    // BGE.W copy; JMP destination. Comparison follows the increment, so the
    // final source longword is included.
    constexpr std::size_t header_bytes = 28;
    constexpr std::uint32_t source_offset = 0x115e;
    constexpr std::uint32_t last_longword_offset = 0x1232;
    constexpr std::uint32_t destination_offset = 0x1d636;
    constexpr std::uint32_t stage_bytes = last_longword_offset - source_offset + 4;
    constexpr std::array<std::uint8_t, 36> entry_bytes{
        0x60, 0x00, 0x00, 0x22, 0x41, 0xf9, 0x00, 0x00, 0x11, 0x5e,
        0x43, 0xf9, 0x00, 0x00, 0x12, 0x32, 0x45, 0xf9, 0x00, 0x01,
        0xd6, 0x36, 0x24, 0xd8, 0xb3, 0xc8, 0x6c, 0x00, 0xff, 0xfa,
        0x4e, 0xf9, 0x00, 0x01, 0xd6, 0x36,
    };
    if (prg.text_bytes != source_offset || prg.data_bytes < stage_bytes
        || prg.bss_bytes < destination_offset + stage_bytes - (prg.text_bytes + prg.data_bytes)) {
        throw std::runtime_error("Unexpected Millennium Atari ST PRG segment layout");
    }
    if (bytes.size() < header_bytes + entry_bytes.size()
        || !std::equal(entry_bytes.begin(), entry_bytes.end(), bytes.begin() + header_bytes)) {
        throw std::runtime_error("Unexpected Millennium Atari ST bootstrap entry");
    }
    return {0, 0x24, source_offset, last_longword_offset, destination_offset, stage_bytes};
}

MillenniumAtariBssEntry parse_millennium_atari_bss_entry(
    std::span<const std::uint8_t> bytes, const AtariStPrg& prg,
    const MillenniumAtariBootstrap& bootstrap) {
    // MOVEA.L #$77000,A1; MOVEA.L #$1d652,A0; MOVE.W #$100,D0;
    // MOVE.W (A0)+,(A1)+; DBF D0,-4; JMP $77000. DBF reaches its body once
    // for D0 = 0, therefore #$100 means 257 words. The source remains an
    // unproven runtime-memory dependency: bootstrap only established 0xd8
    // bytes, not this entire requested range.
    constexpr std::size_t header_bytes = 28;
    constexpr std::uint32_t source_address = 0x1d652;
    constexpr std::uint32_t destination_address = 0x77000;
    constexpr std::uint16_t initial_d0 = 0x100;
    constexpr std::array<std::uint8_t, 28> entry_bytes{
        0x22, 0x7c, 0x00, 0x07, 0x70, 0x00, 0x20, 0x7c, 0x00, 0x01,
        0xd6, 0x52, 0x30, 0x3c, 0x01, 0x00, 0x32, 0xd8, 0x51, 0xc8,
        0xff, 0xfc, 0x4e, 0xf9, 0x00, 0x07, 0x70, 0x00,
    };
    const auto stage_offset = static_cast<std::size_t>(bootstrap.stage_source_offset);
    if (bootstrap.stage_source_offset != prg.text_bytes
        || bootstrap.stage_bytes < entry_bytes.size()
        || stage_offset > bytes.size() - std::min(bytes.size(), header_bytes)
        || header_bytes + stage_offset > bytes.size()
        || entry_bytes.size() > bytes.size() - header_bytes - stage_offset) {
        throw std::runtime_error("Millennium Atari ST BSS entry is outside PRG data");
    }
    const auto begin = bytes.begin() + static_cast<std::ptrdiff_t>(header_bytes + stage_offset);
    if (!std::equal(entry_bytes.begin(), entry_bytes.end(), begin)) {
        throw std::runtime_error("Unexpected Millennium Atari ST BSS entry");
    }
    return {bootstrap.stage_destination_offset, source_address, destination_address,
        initial_d0, static_cast<std::uint32_t>(initial_d0) + 1U, destination_address};
}

MillenniumAtariBssSource materialize_millennium_atari_bss_source(
    std::span<const std::uint8_t> bytes, const AtariStPrg& prg,
    const MillenniumAtariBootstrap& bootstrap, const MillenniumAtariBssEntry& entry) {
    constexpr std::size_t header_bytes = 28;
    const auto loadable_bytes = static_cast<std::uint64_t>(prg.text_bytes) + prg.data_bytes;
    const auto copy_bytes = static_cast<std::uint64_t>(entry.copied_words) * 2U;
    if (entry.entry_offset != bootstrap.stage_destination_offset
        || entry.copy_source_address < entry.entry_offset
        || copy_bytes > std::numeric_limits<std::uint32_t>::max()
        || loadable_bytes > std::numeric_limits<std::uint32_t>::max()
        || entry.entry_offset < loadable_bytes) {
        throw std::runtime_error("Millennium Atari ST BSS source layout is inconsistent");
    }
    const auto load_base = entry.entry_offset - static_cast<std::uint32_t>(loadable_bytes);
    if (load_base > std::numeric_limits<std::uint32_t>::max() - static_cast<std::uint32_t>(loadable_bytes)
        || load_base + static_cast<std::uint32_t>(loadable_bytes) != entry.entry_offset
        || static_cast<std::uint64_t>(entry.copy_source_address) + copy_bytes
            > static_cast<std::uint64_t>(entry.entry_offset) + prg.bss_bytes) {
        throw std::runtime_error("Millennium Atari ST BSS source lies outside loader BSS");
    }
    const auto source_into_stage = entry.copy_source_address - entry.entry_offset;
    if (source_into_stage >= bootstrap.stage_bytes) {
        throw std::runtime_error("Millennium Atari ST BSS source has no bootstrap DATA provenance");
    }
    const auto original_data_bytes = std::min<std::uint64_t>(
        copy_bytes, static_cast<std::uint64_t>(bootstrap.stage_bytes) - source_into_stage);
    const auto source_data_offset = static_cast<std::uint64_t>(bootstrap.stage_source_offset) + source_into_stage;
    if (source_data_offset > loadable_bytes || original_data_bytes > loadable_bytes - source_data_offset
        || header_bytes + source_data_offset > bytes.size()
        || original_data_bytes > bytes.size() - header_bytes - source_data_offset) {
        throw std::runtime_error("Millennium Atari ST BSS source DATA range is outside PRG");
    }

    MillenniumAtariBssSource result;
    result.load_base = load_base;
    result.bss_start_address = entry.entry_offset;
    result.source_address = entry.copy_source_address;
    result.source_data_offset = static_cast<std::uint32_t>(source_data_offset);
    result.original_data_bytes = static_cast<std::uint32_t>(original_data_bytes);
    result.bss_zero_bytes = static_cast<std::uint32_t>(copy_bytes - original_data_bytes);
    result.bytes.resize(static_cast<std::size_t>(copy_bytes));
    std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(header_bytes + source_data_offset),
        static_cast<std::ptrdiff_t>(original_data_bytes), result.bytes.begin());
    return result;
}

MillenniumAtariMaterializedTarget materialize_millennium_atari_target(
    const MillenniumAtariBssSource& source, const MillenniumAtariBssEntry& entry) {
    // MOVE.W #$2,-(A7); MOVE.L #$1d6e4,-(A7); MOVE.W #$3d,-(A7).
    // The following original TRAP #1 is evidence only and is never invoked.
    constexpr std::array<std::uint8_t, 14> target_prefix{
        0x3f, 0x3c, 0x00, 0x02, 0x2f, 0x3c, 0x00, 0x01, 0xd6, 0xe4,
        0x3f, 0x3c, 0x00, 0x3d,
    };
    const auto expected_bytes = static_cast<std::uint64_t>(entry.copied_words) * 2U;
    if (source.source_address != entry.copy_source_address
        || entry.copy_destination_address != entry.jump_address
        || expected_bytes != source.bytes.size()
        || source.bytes.size() < target_prefix.size()
        || !std::equal(target_prefix.begin(), target_prefix.end(), source.bytes.begin())) {
        throw std::runtime_error("Unexpected Millennium Atari ST materialized target");
    }
    MillenniumAtariMaterializedTarget result;
    result.source_address = source.source_address;
    result.target_address = entry.copy_destination_address;
    result.first_opcode = read_be16(source.bytes, 0);
    result.first_immediate_word = read_be16(source.bytes, 2);
    result.first_immediate_longword = read_be32(source.bytes, 6);
    result.bytes = source.bytes;
    return result;
}

} // namespace eon
