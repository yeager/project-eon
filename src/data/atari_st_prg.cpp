#include "data/atari_st_prg.hpp"

#include "data/sha256.hpp"

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

MillenniumAtariTrapEntry parse_millennium_atari_trap_entry(
    const MillenniumAtariBssSource& source, const MillenniumAtariMaterializedTarget& target) {
    // MOVE.W #2,-(A7); MOVE.L #$1d6e4,-(A7); MOVE.W #$3d,-(A7); TRAP #1;
    // MOVE.W D0,-(A7); MOVE.W #$3e,-(A7); TST.L D0; BMI.S -2.
    // $3d is the invoked GEMDOS Fopen selector. $3e is only prepared here;
    // no second TRAP occurs in this proven range. The Fopen result is
    // OS-owned, so the final BMI is retained as a branch fact only.
    constexpr std::uint32_t filename_address = 0x1d6e4;
    constexpr std::uint16_t fopen_mode = 2;
    constexpr std::uint16_t fopen_function = 0x3d;
    constexpr std::uint16_t fclose_function = 0x3e;
    constexpr std::array<std::uint8_t, 26> entry_bytes{
        0x3f, 0x3c, 0x00, 0x02, 0x2f, 0x3c, 0x00, 0x01, 0xd6, 0xe4,
        0x3f, 0x3c, 0x00, 0x3d, 0x4e, 0x41, 0x3f, 0x00, 0x3f, 0x3c,
        0x00, 0x3e, 0x4a, 0x80, 0x6b, 0xfe,
    };
    if (target.target_address == 0 || source.source_address > filename_address
        || filename_address - source.source_address >= source.bytes.size()
        || target.bytes.size() < entry_bytes.size()
        || !std::equal(entry_bytes.begin(), entry_bytes.end(), target.bytes.begin())) {
        throw std::runtime_error("Unexpected Millennium Atari ST GEMDOS entry");
    }
    const auto filename_offset = static_cast<std::size_t>(filename_address - source.source_address);
    const auto terminator = std::find(source.bytes.begin() + static_cast<std::ptrdiff_t>(filename_offset),
        source.bytes.end(), static_cast<std::uint8_t>(0));
    if (terminator == source.bytes.end() || terminator == source.bytes.begin() + static_cast<std::ptrdiff_t>(filename_offset)) {
        throw std::runtime_error("Unterminated Millennium Atari ST GEMDOS filename");
    }
    MillenniumAtariTrapEntry result;
    result.target_address = target.target_address;
    result.fopen_filename_address = filename_address;
    result.fopen_filename.assign(source.bytes.begin() + static_cast<std::ptrdiff_t>(filename_offset), terminator);
    result.fopen_access_mode = fopen_mode;
    result.fopen_function = fopen_function;
    result.fopen_trap_offset = 14;
    result.following_fclose_function = fclose_function;
    result.following_fclose_selector_offset = 18;
    result.fopen_result_test_offset = 22;
    result.fopen_result_negative_branch_offset = 24;
    result.fopen_result_negative_branch_target_offset = 24;
    return result;
}

MillenniumAtariConfigEvidence probe_millennium_atari_config(const Fat12Disk& disk) {
    constexpr std::string_view requested_filename = "MILL22A.inf";
    MillenniumAtariConfigEvidence result;
    result.requested_filename = requested_filename;
    result.root_entry_count = disk.root_entries().size();
    if (const auto* entry = disk.find(requested_filename)) {
        if (entry->directory()) {
            throw std::runtime_error("Millennium Atari ST configuration name is a directory");
        }
        result.present = true;
        result.first_cluster = entry->first_cluster;
        result.size = entry->size;
        const auto payload = disk.read(*entry);
        if (payload.size() != result.size || payload.size() < 2U) {
            throw std::runtime_error("Truncated Millennium Atari ST configuration payload");
        }
        result.sha256 = to_hex(sha256(payload));
        result.first_word = read_be16(payload, 0);
        if (payload.size() >= 6U) result.first_longword_operand = read_be32(payload, 2);
    }
    return result;
}

MillenniumAtariConfigEntry parse_millennium_atari_config_entry(
    std::span<const std::uint8_t> payload) {
    // The original file starts JMP $2aa88.  Its absolute references at the
    // beginning of the payload establish the observed $2a4de load base, so
    // the jump resolves to file +$5aa.  From there the sequence is:
    // CLR.L -(A7); MOVE.W #$15,-(A7); TRAP #14; ADDQ.L #6,A7;
    // MOVE.L #$2a612,-(A7); MOVE.W #6,-(A7); TRAP #14; ADDQ.L #6,A7;
    // then six JSRs interleaved with literal register setup; PEA $2ab0a;
    // MOVE.W #$26,-(A7); TRAP #14; ADDQ.L #6,A7; RTS.
    // All service selectors remain interface facts: their TOS/XBIOS effects
    // and every called routine remain outside this parser.
    constexpr std::uint32_t load_base = 0x2a4de;
    constexpr std::uint32_t entry_address = 0x2aa88;
    constexpr std::size_t entry_offset = entry_address - load_base;
    constexpr auto entry_bytes = std::to_array<std::uint8_t>({
        0x42, 0xa7, 0x3f, 0x3c, 0x00, 0x15, 0x4e, 0x4e, 0x5c, 0x8f,
        0x2f, 0x3c, 0x00, 0x02, 0xa6, 0x12, 0x3f, 0x3c, 0x00, 0x06,
        0x4e, 0x4e, 0x5c, 0x8f, 0x4e, 0xb9, 0x00, 0x02, 0xb5, 0x5a,
        0x4e, 0xb9, 0x00, 0x02, 0xaa, 0x68, 0x2e, 0x3c, 0x00, 0x02,
        0xa6, 0x40, 0x4e, 0xb9, 0x00, 0x02, 0xaa, 0x0c, 0x28, 0x7c,
        0x00, 0x02, 0xc2, 0x4a, 0x54, 0x8c, 0x3c, 0x1c, 0x3e, 0x1c,
        0x2a, 0x79, 0x00, 0x02, 0xa5, 0x0e, 0x4e, 0xb9, 0x00, 0x02,
        0xb2, 0xbe, 0x26, 0x7c, 0x00, 0x02, 0xa6, 0x4c, 0x28, 0x7c,
        0x00, 0x02, 0xa6, 0x6c, 0x4e, 0xb9, 0x00, 0x02, 0xb4, 0x48,
        0x2e, 0x3c, 0x00, 0x02, 0xa6, 0x34, 0x4e, 0xb9, 0x00, 0x02,
        0xaa, 0x0c, 0x26, 0x7c, 0x00, 0x02, 0xa6, 0x4c, 0x28, 0x7c,
        0x00, 0x02, 0xa6, 0x6c, 0x48, 0x7a, 0x00, 0x0c, 0x3f, 0x3c,
        0x00, 0x26, 0x4e, 0x4e, 0x5c, 0x8f, 0x4e, 0x75,
    });
    constexpr std::array<std::uint32_t, 6> jsr_targets{
        0x2b55a, 0x2aa68, 0x2aa0c, 0x2b2be, 0x2b448, 0x2aa0c,
    };
    if (payload.size() < 6 || read_be16(payload, 0) != 0x4ef9U
        || read_be32(payload, 2) != entry_address
        || payload.size() < entry_offset + entry_bytes.size()
        || !std::equal(entry_bytes.begin(), entry_bytes.end(),
            payload.begin() + static_cast<std::ptrdiff_t>(entry_offset))) {
        throw std::runtime_error("Unexpected Millennium Atari ST MILL22A.inf entry path");
    }
    MillenniumAtariConfigEntry result;
    result.proven_load_base = load_base;
    result.entry_address = entry_address;
    result.entry_file_offset = static_cast<std::uint32_t>(entry_offset);
    result.initial_trap_selector = 0x15;
    result.initial_trap_longword_argument = 0;
    result.palette_trap_selector = 0x06;
    result.palette_trap_longword_argument = 0x2a612;
    result.jsr_targets.assign(jsr_targets.begin(), jsr_targets.end());
    result.final_pea_address = load_base + 0x62c;
    result.final_trap_selector = 0x26;
    result.return_offset = static_cast<std::uint32_t>(entry_offset + entry_bytes.size() - 2U);
    return result;
}

MillenniumAtariConfigFirstJsr parse_millennium_atari_config_first_jsr(
    std::span<const std::uint8_t> payload, const MillenniumAtariConfigEntry& entry) {
    // $2b55a is the first literal JSR target at the established $2a4de base.
    // Its leading dynamic-bit opcode is retained without attributing a result
    // to it: that result depends on caller state. The following MOVEM.L
    // (A7)+,D0-D7/A0-A6 and RTS are complete, local machine-code facts.
    constexpr std::uint32_t load_base = 0x2a4de;
    constexpr std::uint32_t target_address = 0x2b55a;
    constexpr std::size_t target_offset = target_address - load_base;
    constexpr std::array<std::uint8_t, 8> target_bytes{
        0x03, 0x5a, 0x4c, 0xdf, 0x7f, 0xff, 0x4e, 0x75,
    };
    if (entry.proven_load_base != load_base || entry.jsr_targets.empty()
        || entry.jsr_targets.front() != target_address
        || payload.size() < target_offset + target_bytes.size()
        || !std::equal(target_bytes.begin(), target_bytes.end(),
            payload.begin() + static_cast<std::ptrdiff_t>(target_offset))) {
        throw std::runtime_error("Unexpected Millennium Atari ST first MILL22A.inf JSR target");
    }
    return {load_base, target_address, static_cast<std::uint32_t>(target_offset),
        read_be16(payload, target_offset), read_be16(payload, target_offset + 2U),
        read_be16(payload, target_offset + 4U), read_be16(payload, target_offset + 6U)};
}

MillenniumAtariConfigSecondJsr parse_millennium_atari_config_second_jsr(
    std::span<const std::uint8_t> payload, const MillenniumAtariConfigEntry& entry) {
    // $2aa68 is the second literal JSR target. It begins with an immediate
    // bit-operation on D0 and an EQ short branch. The branch skips the exact
    // 20-byte middle path to the JSR at $2aa82. Both paths join at its
    // literal target $2a51c, then fall through to the entry block at $2aa88
    // whose first JSR is at $2aaa0. All targets remain control facts only.
    constexpr std::uint32_t load_base = 0x2a4de;
    constexpr std::uint32_t target_address = 0x2aa68;
    constexpr std::size_t target_offset = target_address - load_base;
    constexpr std::array<std::uint8_t, 32> target_bytes{
        0x08, 0x80, 0x00, 0x0d, 0x67, 0x14, 0x41, 0xf8, 0x88, 0x00,
        0x30, 0x3c, 0x07, 0xff, 0x01, 0x88, 0x00, 0x00, 0x10, 0xbc,
        0x00, 0x0e, 0x46, 0xfc, 0x03, 0x00, 0x4e, 0xb9, 0x00, 0x02,
        0xa5, 0x1c,
    };
    constexpr std::uint32_t join_jsr_address = 0x2aa82;
    constexpr std::uint32_t join_jsr_target = 0x2a51c;
    constexpr std::uint32_t following_jsr_target = 0x2b55a;
    if (entry.proven_load_base != load_base || entry.jsr_targets.size() < 2U
        || entry.jsr_targets[1] != target_address
        || payload.size() < target_offset + 62U
        || !std::equal(target_bytes.begin(), target_bytes.end(),
            payload.begin() + static_cast<std::ptrdiff_t>(target_offset))
        || read_be32(payload, target_offset + 28U) != join_jsr_target
        || read_be16(payload, target_offset + 56U) != 0x4eb9U
        || read_be32(payload, target_offset + 58U) != following_jsr_target) {
        throw std::runtime_error("Unexpected Millennium Atari ST second MILL22A.inf JSR target");
    }
    return {load_base, target_address, static_cast<std::uint32_t>(target_offset),
        read_be16(payload, target_offset), read_be16(payload, target_offset + 2U),
        read_be16(payload, target_offset + 4U), target_address + 6U + 0x14U,
        join_jsr_address, join_jsr_target, following_jsr_target};
}

MillenniumAtariConfigJoinJsr parse_millennium_atari_config_join_jsr(
    std::span<const std::uint8_t> payload, const MillenniumAtariConfigSecondJsr& second) {
    // The common $2a51c target ends locally at RTS. Its intervening $a000 is
    // preserved as an opaque Line-A opcode: the original platform owns its
    // effect, and no host implementation is selected here.
    constexpr std::uint32_t load_base = 0x2a4de;
    constexpr std::uint32_t target_address = 0x2a51c;
    constexpr std::size_t target_offset = target_address - load_base;
    constexpr std::array<std::uint8_t, 32> target_bytes{
        0x54, 0x8f, 0x33, 0xc0, 0x00, 0x02, 0xa5, 0x12, 0xa0, 0x00,
        0x26, 0x68, 0x00, 0x08, 0x28, 0x68, 0x00, 0x0c, 0x23, 0xcb,
        0x00, 0x02, 0xa5, 0x14, 0x23, 0xcc, 0x00, 0x02, 0xa5, 0x18,
        0x4e, 0x75,
    };
    if (second.proven_load_base != load_base || second.join_jsr_target != target_address
        || payload.size() < target_offset + target_bytes.size()
        || !std::equal(target_bytes.begin(), target_bytes.end(),
            payload.begin() + static_cast<std::ptrdiff_t>(target_offset))) {
        throw std::runtime_error("Unexpected Millennium Atari ST common MILL22A.inf JSR target");
    }
    return {load_base, target_address, static_cast<std::uint32_t>(target_offset),
        read_be16(payload, target_offset), read_be16(payload, target_offset + 2U),
        read_be32(payload, target_offset + 4U), read_be16(payload, target_offset + 8U),
        read_be32(payload, target_offset + 20U), read_be32(payload, target_offset + 26U),
        read_be16(payload, target_offset + 30U)};
}

} // namespace eon
