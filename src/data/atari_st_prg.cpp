#include "data/atari_st_prg.hpp"

#include "data/sha256.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <tuple>

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

MillenniumAtariBootstrapExecution execute_millennium_atari_bootstrap_prefix(
    std::span<const std::uint8_t> bytes, const AtariStPrg& prg,
    const MillenniumAtariBootstrap& bootstrap, const MillenniumAtariBssEntry& entry) {
    // The entry uses MOVE.L (A0)+,(A2)+ followed by a BGE backedge. Its
    // source/last-longword relation has already been validated, but execute
    // each original longword instead of treating this as opaque host copying.
    constexpr std::size_t header_bytes = 28;
    if (bootstrap.entry_offset != 0 || bootstrap.branch_target_offset != 0x24
        || bootstrap.stage_bytes == 0 || bootstrap.stage_bytes % 4U != 0U
        || header_bytes + static_cast<std::size_t>(bootstrap.stage_source_offset) > bytes.size()
        || bootstrap.stage_bytes > bytes.size() - header_bytes - bootstrap.stage_source_offset) {
        throw std::runtime_error("Unsupported Millennium Atari ST local bootstrap execution");
    }
    MillenniumAtariBootstrapExecution result;
    result.initial_pc_offset = bootstrap.entry_offset;
    result.branch_pc_offset = bootstrap.branch_target_offset;
    result.first_copy_longwords = bootstrap.stage_bytes / 4U;
    result.bss_entry_address = bootstrap.stage_destination_offset;
    result.copied_stage_bytes.resize(bootstrap.stage_bytes);
    const auto source = bytes.subspan(header_bytes + bootstrap.stage_source_offset,
        bootstrap.stage_bytes);
    for (std::size_t longword = 0; longword < result.first_copy_longwords; ++longword) {
        const auto offset = longword * 4U;
        std::copy_n(source.begin() + static_cast<std::ptrdiff_t>(offset), 4,
            result.copied_stage_bytes.begin() + static_cast<std::ptrdiff_t>(offset));
    }
    if (!std::equal(result.copied_stage_bytes.begin(), result.copied_stage_bytes.end(), source.begin())) {
        throw std::runtime_error("Millennium Atari ST first copy did not preserve original bytes");
    }

    const auto bss_source = materialize_millennium_atari_bss_source(bytes, prg, bootstrap, entry);
    const auto source_into_stage = entry.copy_source_address - bootstrap.stage_destination_offset;
    if (entry.entry_offset != bootstrap.stage_destination_offset
        || source_into_stage > result.copied_stage_bytes.size()
        || bss_source.original_data_bytes > result.copied_stage_bytes.size() - source_into_stage
        || bss_source.bytes.size() != static_cast<std::size_t>(entry.copied_words) * 2U) {
        throw std::runtime_error("Unsupported Millennium Atari ST second local copy execution");
    }
    if (!std::equal(bss_source.bytes.begin(),
            bss_source.bytes.begin() + static_cast<std::ptrdiff_t>(bss_source.original_data_bytes),
            result.copied_stage_bytes.begin() + static_cast<std::ptrdiff_t>(source_into_stage))) {
        throw std::runtime_error("Millennium Atari ST BSS source lost original DATA provenance");
    }

    result.second_copy_words = entry.copied_words;
    result.target.source_address = entry.copy_source_address;
    result.target.target_address = entry.copy_destination_address;
    result.target.bytes.resize(bss_source.bytes.size());
    for (std::size_t word = 0; word < result.second_copy_words; ++word) {
        const auto offset = word * 2U;
        result.target.bytes[offset] = bss_source.bytes[offset];
        result.target.bytes[offset + 1U] = bss_source.bytes[offset + 1U];
    }
    result.target.first_opcode = read_be16(result.target.bytes, 0);
    result.target.first_immediate_word = read_be16(result.target.bytes, 2);
    result.target.first_immediate_longword = read_be32(result.target.bytes, 6);
    if (result.target.bytes != bss_source.bytes || result.target.target_address != entry.jump_address) {
        throw std::runtime_error("Millennium Atari ST second copy did not reach expected target");
    }
    // The fixed target prefix reaches TRAP #1 at byte +14. Do not execute it
    // or construct the preceding A7 service frame.
    result.target_address = result.target.target_address;
    result.stop_before_trap_address = result.target_address + 14U;
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

MillenniumAtariFopenFallthrough parse_millennium_atari_fopen_fallthrough(
    const MillenniumAtariMaterializedTarget& target, const MillenniumAtariTrapEntry& trap) {
    // The negative Fopen branch is self-referential at +$18. Its nonnegative
    // fall-through begins at +$1a and pushes a literal buffer/count, the
    // D0-owned handle word, and selector $3f before TRAP #1. ADDA.L #12,A7
    // is retained as post-service stack cleanup only; no service result is
    // read or emulated by this parser.
    constexpr std::size_t entry_offset = 0x1a;
    constexpr std::array<std::uint8_t, 26> entry_bytes{
        0x2f, 0x3c, 0x00, 0x02, 0xa5, 0x00,
        0x2f, 0x3c, 0x00, 0x02, 0x00, 0x00,
        0x3f, 0x00, 0x3f, 0x3c, 0x00, 0x3f,
        0x4e, 0x41, 0xdf, 0xfc, 0x00, 0x00, 0x00, 0x0c,
    };
    constexpr std::string_view expected_sha256 =
        "663d5f1418326aa9c0efde064ad95bda21c84d7f23241ce3505f21f1f07474d0";
    if (target.target_address == 0 || trap.target_address != target.target_address
        || trap.fopen_result_negative_branch_offset != 0x18
        || trap.fopen_result_negative_branch_target_offset != 0x18
        || target.bytes.size() < entry_offset + entry_bytes.size()
        || !std::equal(entry_bytes.begin(), entry_bytes.end(),
            target.bytes.begin() + static_cast<std::ptrdiff_t>(entry_offset))) {
        throw std::runtime_error("Unexpected Millennium Atari ST Fopen fall-through");
    }
    const auto bytes = std::span(target.bytes).subspan(entry_offset, entry_bytes.size());
    const auto digest = to_hex(sha256(bytes));
    if (digest != expected_sha256) {
        throw std::runtime_error("Unexpected Millennium Atari ST Fopen fall-through hash");
    }
    return {target.target_address, entry_offset, entry_bytes.size(), digest,
        read_be32(bytes, 2), read_be32(bytes, 8), read_be16(bytes, 12), read_be16(bytes, 16),
        entry_offset + 18U, read_be16(bytes, 20), read_be32(bytes, 22)};
}

MillenniumAtariFreadConfigTransferBoundary
parse_millennium_atari_fread_config_transfer_boundary(
    const MillenniumAtariMaterializedTarget& target,
    const MillenniumAtariFopenFallthrough& fallthrough) {
    // The preceding boundary ends its literal Fread preparation at +$2c and
    // retains the following ADDA.L #12,SP at +$2e. The next original bytes
    // execute TRAP #1, repeat that cleanup, then JSR to the literal Fread
    // destination. Neither native call is made here, so this is deliberately
    // an edge encoding rather than a runnable configuration transition.
    constexpr std::size_t entry_offset = 0x34;
    constexpr std::array<std::uint8_t, 14> entry_bytes{
        0x4e, 0x41, 0xdf, 0xfc, 0x00, 0x00, 0x00, 0x0c,
        0x4e, 0xb9, 0x00, 0x02, 0xa5, 0x00,
    };
    constexpr std::string_view expected_sha256 =
        "845d677c7c17d2152f0e89e0a396b6bbfb1ed6a75479a325b39310bbf0d99e58";
    if (target.target_address == 0
        || fallthrough.target_address != target.target_address
        || fallthrough.entry_offset != 0x1a
        || fallthrough.byte_count != 26
        || fallthrough.fread_trap_offset != 0x2c
        || fallthrough.stack_cleanup_opcode != 0xdffc
        || fallthrough.stack_cleanup_bytes != 12
        || target.bytes.size() < entry_offset + entry_bytes.size()
        || !std::equal(entry_bytes.begin(), entry_bytes.end(),
            target.bytes.begin() + static_cast<std::ptrdiff_t>(entry_offset))) {
        throw std::runtime_error("Unexpected Millennium Atari ST Fread configuration transfer boundary");
    }
    const auto bytes = std::span(target.bytes).subspan(entry_offset, entry_bytes.size());
    const auto digest = to_hex(sha256(bytes));
    if (digest != expected_sha256) {
        throw std::runtime_error("Unexpected Millennium Atari ST Fread configuration transfer boundary hash");
    }
    return {target.target_address, entry_offset, entry_bytes.size(), digest,
        read_be16(bytes, 0), read_be16(bytes, 2), read_be32(bytes, 4), read_be16(bytes, 8),
        read_be32(bytes, 10)};
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

MillenniumAtariRootInventory inventory_millennium_atari_equinox_root(const Fat12Disk& disk) {
    // This ordered table is a physical FAT12-root fact from the verified
    // Equinox image, not an inferred load order or game-data schema.
    constexpr std::array<std::tuple<std::string_view, std::uint16_t, std::uint32_t,
        std::string_view>, 13> expected{{
        {"DESKTOP.INF", 2, 555, "ce2aa85b442be281f25c22456c0d081d01b51108e96716bba9f867b7e791ab19"},
        {"MILL22A.INF", 3, 7506, "74d7d630779fd811aedcdbe31b14e54198eb9ffd673df512dd70b6165c4a37b6"},
        {"MILL22B.INF", 11, 84720, "e315b0ec01f2fe429fdce101765577b893d031389c540de1fbe43eca121d53e9"},
        {"MILL22C.INF", 94, 9597, "a28a49eea33a14210193bbe6e36abf95700ac6789681bf1a9eac5d09a0999055"},
        {"MILL22D.INF", 104, 18428, "de0a95d3e4659a305b3e55b3417a7648127b41866de0a0ca344a81c66979dbc0"},
        {"MILL22E.INF", 122, 302892, "9aeb6aafceab228521725ffe687cd3d95406d7f272bca77f855ebb600664b2af"},
        {"MILL22F.INF", 418, 22123, "26ef995a9c6a43647e7905477168980159d1426d90f901d4f4c32f7cf13e455e"},
        {"2200SAVE.I", 440, 7313, "b0b91572a7cc8ca0b7b112a8ce09bcf0c6645c6b32df836ae8c2eb27d86c333a"},
        {"2200SAVE.II", 448, 7313, "fa11ee72b3ca009d8a5d6cece8ff3f95b01b29ed53106e2d3730c9a545400065"},
        {"2200SAVE.III", 456, 7313, "54519e0eebfe3f3a38b04e4b372caf67476148c135dafbfe8d0a4bcae601eae2"},
        {"2200SAVE.IV", 464, 7313, "8c1709bb7aba3adc2e6538867383229c4d6a285d29a78fb431970d0d926ffbd2"},
        {"DATA12.BIN", 442, 932, "6f1e8ab7720c530f8cf5bfc07497824ff731ce977a15d941dad5acd999c6eeda"},
        {"MILENIUM.TOS", 540, 49269, "4584ddc459e3bf03e642f3156fbedb74aa33a847db4937beb5635eb492e93686"},
    }};
    if (disk.root_entries().size() != expected.size()) {
        throw std::runtime_error("Unexpected Millennium Atari ST Equinox root entry count");
    }
    MillenniumAtariRootInventory result;
    result.files.reserve(expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
        const auto& [name, first_cluster, size, expected_hash] = expected[index];
        const auto& entry = disk.root_entries()[index];
        if (entry.directory() || entry.name != name || entry.first_cluster != first_cluster
            || entry.size != size) {
            throw std::runtime_error("Unexpected Millennium Atari ST Equinox root entry");
        }
        const auto bytes = disk.read(entry);
        if (bytes.size() != size) {
            throw std::runtime_error("Truncated Millennium Atari ST Equinox root file");
        }
        const auto digest = to_hex(sha256(bytes));
        if (digest != expected_hash) {
            throw std::runtime_error("Unexpected Millennium Atari ST Equinox root file hash");
        }
        result.files.push_back({std::string(name), first_cluster, size, digest});
    }
    return result;
}

MillenniumAtariAuxiliaryResourceNameEvidence
probe_millennium_atari_auxiliary_resource_name(const Fat12Disk& disk) {
    constexpr std::string_view container_filename = "MILL22B.INF";
    constexpr std::string_view container_hash =
        "e315b0ec01f2fe429fdce101765577b893d031389c540de1fbe43eca121d53e9";
    constexpr std::uint32_t literal_offset = 0x11600;
    constexpr std::array<std::uint8_t, 12> expected_literal{{
        'M', 'I', 'L', 'L', '2', '2', 'E', '.', 'I', 'N', 'F', 0,
    }};
    constexpr std::array<std::uint8_t, 14> expected_preceding{{
        0x33, 0xdc, 0x00, 0x01, 0x44, 0xc8, 0x23, 0xcc, 0x00, 0x01,
        0x44, 0xc4, 0x4e, 0x75,
    }};
    const auto* entry = disk.find(container_filename);
    if (!entry || entry->directory()) {
        throw std::runtime_error("Millennium Atari ST auxiliary container is absent");
    }
    const auto payload = disk.read(*entry);
    if (to_hex(sha256(payload)) != container_hash || payload.size() != entry->size
        || literal_offset < expected_preceding.size()
        || literal_offset > payload.size() || expected_literal.size() > payload.size() - literal_offset
        || !std::equal(expected_literal.begin(), expected_literal.end(),
            payload.begin() + static_cast<std::ptrdiff_t>(literal_offset))
        || !std::equal(expected_preceding.begin(), expected_preceding.end(),
            payload.begin() + static_cast<std::ptrdiff_t>(literal_offset - expected_preceding.size()))) {
        throw std::runtime_error("Unexpected Millennium Atari ST auxiliary resource-name evidence");
    }
    return {std::string(container_filename), entry->first_cluster, entry->size,
        std::string(container_hash), literal_offset, "MILL22E.INF", literal_offset - 2};
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
    result.second_trap_selector = 0x06;
    result.second_trap_longword_argument = 0x2a612;
    result.jsr_targets.assign(jsr_targets.begin(), jsr_targets.end());
    result.final_pea_address = load_base + 0x62c;
    result.final_trap_selector = 0x26;
    result.return_offset = static_cast<std::uint32_t>(entry_offset + entry_bytes.size() - 2U);
    return result;
}

MillenniumAtariFreadConfigLoadAddressBoundary
parse_millennium_atari_fread_config_load_address_boundary(
    const MillenniumAtariFreadConfigTransferBoundary& transfer,
    const std::span<const std::uint8_t> payload,
    const MillenniumAtariConfigEntry& independent_entry) {
    // A literal Fread-to-$2a500 followed by JSR $2a500 establishes an
    // original loader edge, but it does not establish the address at which
    // disk-file bytes are resident after GEMDOS returns.  The supplied file's
    // first six bytes are JMP $2aa88.  If its byte zero were at $2a500, that
    // target would be file +$588; the independently byte-verified candidate
    // entry is file +$5aa at base $2a4de.  Preserve that 34-byte mismatch as
    // evidence and deliberately do not resolve it into an execution model.
    constexpr std::uint32_t fread_destination = 0x2a500;
    constexpr std::array<std::uint8_t, 6> initial_jump_bytes{
        0x4e, 0xf9, 0x00, 0x02, 0xaa, 0x88,
    };
    constexpr std::uint32_t initial_jump_target = 0x2aa88;
    constexpr std::uint32_t candidate_load_base = 0x2a4de;
    constexpr std::uint32_t candidate_entry_offset = 0x5aa;
    constexpr std::int32_t expected_delta = 34;
    constexpr std::string_view expected_sha256 =
        "5c2fb1d412ca66ba8928a77c22eb0351ab5d3d6fd9c04cff1b037f25a94c7829";
    if (transfer.config_buffer_address != fread_destination
        || transfer.config_jsr_opcode != 0x4eb9U
        || payload.size() < initial_jump_bytes.size()
        || !std::equal(initial_jump_bytes.begin(), initial_jump_bytes.end(), payload.begin())
        || independent_entry.proven_load_base != candidate_load_base
        || independent_entry.entry_address != initial_jump_target
        || independent_entry.entry_file_offset != candidate_entry_offset) {
        throw std::runtime_error("Unexpected Millennium Atari ST Fread configuration load-address boundary");
    }
    const auto header = payload.first(initial_jump_bytes.size());
    const auto digest = to_hex(sha256(header));
    const auto direct_offset = initial_jump_target - fread_destination;
    const auto delta = static_cast<std::int32_t>(candidate_entry_offset)
        - static_cast<std::int32_t>(direct_offset);
    if (digest != expected_sha256 || direct_offset >= payload.size() || delta != expected_delta) {
        throw std::runtime_error("Unexpected Millennium Atari ST Fread configuration load-address boundary hash");
    }
    return {fread_destination, read_be16(header, 0), read_be32(header, 2), direct_offset,
        digest, candidate_load_base, candidate_entry_offset, delta};
}

MillenniumAtariFreadMappedConfigPrelude parse_millennium_atari_fread_mapped_config_prelude(
    const MillenniumAtariFreadConfigTransferBoundary& transfer,
    const std::span<const std::uint8_t> payload,
    const MillenniumAtariConfigEntry& independent_entry) {
    // The Fread boundary literally names $2a500 and the supplied payload
    // begins JMP $2aa88.  Under that *conditional* byte-zero mapping, the
    // target is file +$588, not +$5aa.  The intervening 34 bytes are complete
    // original code: their SR-dependent branch and fall-through converge at
    // JSR $2a51c, then the return site falls through to file +$5aa.  We bind
    // the relationship without turning the unrecovered Fread return or JSR
    // into an executable host path.
    constexpr std::uint32_t fread_destination = 0x2a500;
    constexpr std::uint32_t mapped_entry = 0x2aa88;
    constexpr std::size_t mapped_offset = mapped_entry - fread_destination;
    constexpr std::size_t byte_count = 34;
    constexpr std::uint32_t branch_target = 0x2aaa4;
    constexpr std::uint32_t jsr_address = 0x2aaa4;
    constexpr std::array<std::uint8_t, byte_count> expected_bytes{
        0x40, 0xc0, 0x08, 0x80, 0x00, 0x0d, 0x67, 0x14,
        0x41, 0xf8, 0x88, 0x00, 0x30, 0x3c, 0x07, 0xff,
        0x01, 0x88, 0x00, 0x00, 0x10, 0xbc, 0x00, 0x0e,
        0x46, 0xfc, 0x03, 0x00, 0x4e, 0xb9, 0x00, 0x02,
        0xa5, 0x1c,
    };
    constexpr std::string_view expected_sha256 =
        "dede20eddbd8015da1d1a4f2f5e53424c2bc2195bff238d830ea24c9f522ea59";
    if (transfer.config_buffer_address != fread_destination
        || transfer.config_jsr_opcode != 0x4eb9U
        || payload.size() < mapped_offset + byte_count
        || independent_entry.entry_file_offset != 0x5aaU
        || !std::equal(expected_bytes.begin(), expected_bytes.end(),
            payload.begin() + static_cast<std::ptrdiff_t>(mapped_offset))) {
        throw std::runtime_error("Unexpected Millennium Atari ST Fread-mapped configuration prelude");
    }
    const auto bytes = payload.subspan(mapped_offset, byte_count);
    const auto digest = to_hex(sha256(bytes));
    if (digest != expected_sha256) {
        throw std::runtime_error("Unexpected Millennium Atari ST Fread-mapped configuration prelude hash");
    }
    return {fread_destination, mapped_entry, static_cast<std::uint32_t>(mapped_offset),
        fread_destination + independent_entry.entry_file_offset, byte_count, digest,
        read_be16(bytes, 0), read_be16(bytes, 6), branch_target, jsr_address,
        read_be16(bytes, 28), read_be32(bytes, 30)};
}

MillenniumAtariConfigTrapArgumentStrings parse_millennium_atari_config_trap_argument_strings(
    const std::span<const std::uint8_t> payload, const MillenniumAtariConfigEntry& entry) {
    // The entry's second literal TRAP argument is $2a612. At the established
    // $2a4de load base this is file +$134, where the original contiguous
    // bytes contain exactly two NUL-terminated strings. Their use is unknown.
    constexpr std::uint32_t load_base = 0x2a4de;
    constexpr std::uint32_t argument_address = 0x2a612;
    constexpr std::size_t argument_offset = argument_address - load_base;
    constexpr auto argument_bytes = std::to_array<std::uint8_t>({
        'M', 'I', 'L', 'L', '2', '2', 'D', '.', 'I', 'N', 'F', 0x00,
        'M', 'I', 'L', 'L', '2', '2', 'C', '.', 'I', 'N', 'F', 0x00,
    });
    constexpr std::string_view expected_sha256 =
        "815bea3862908e01557486cae7d42132853c94348b49b920f9d3e88e14956c51";
    if (entry.proven_load_base != load_base || entry.second_trap_selector != 0x06
        || entry.second_trap_longword_argument != argument_address
        || payload.size() < argument_offset + argument_bytes.size()
        || !std::equal(argument_bytes.begin(), argument_bytes.end(),
            payload.begin() + static_cast<std::ptrdiff_t>(argument_offset))) {
        throw std::runtime_error("Unexpected Millennium Atari ST MILL22A.inf literal TRAP argument");
    }
    const auto bytes = payload.subspan(argument_offset, argument_bytes.size());
    const auto hash = to_hex(sha256(bytes));
    if (hash != expected_sha256) {
        throw std::runtime_error("Unexpected Millennium Atari ST MILL22A.inf literal TRAP argument hash");
    }
    return {load_base, argument_address, static_cast<std::uint32_t>(argument_offset),
        {"MILL22D.INF", "MILL22C.INF"}, hash};
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

MillenniumAtariConfigForwardedJsr parse_millennium_atari_config_forwarded_jsr(
    std::span<const std::uint8_t> payload, const MillenniumAtariConfigEntry& entry) {
    // Both original entry-block calls to $2aa0c arrive at JMP $2a5dc. Its
    // 12-byte destination body ends locally; selector $19 stays an interface
    // fact because its TRAP #14 behaviour belongs to the original platform.
    constexpr std::uint32_t load_base = 0x2a4de;
    constexpr std::uint32_t entry_address = 0x2aa0c;
    constexpr std::size_t entry_offset = entry_address - load_base;
    constexpr std::uint32_t forwarded_address = 0x2a5dc;
    constexpr std::size_t forwarded_offset = forwarded_address - load_base;
    constexpr std::array<std::uint8_t, 6> entry_bytes{0x4e, 0xf9, 0x00, 0x02, 0xa5, 0xdc};
    constexpr std::array<std::uint8_t, 12> forwarded_bytes{
        0x3f, 0x01, 0x3f, 0x3c, 0x00, 0x19, 0x4e, 0x4e, 0x50, 0x4f, 0x4e, 0x75,
    };
    const auto has_target = std::find(entry.jsr_targets.begin(), entry.jsr_targets.end(), entry_address)
        != entry.jsr_targets.end();
    if (entry.proven_load_base != load_base || !has_target
        || payload.size() < entry_offset + entry_bytes.size()
        || payload.size() < forwarded_offset + forwarded_bytes.size()
        || !std::equal(entry_bytes.begin(), entry_bytes.end(),
            payload.begin() + static_cast<std::ptrdiff_t>(entry_offset))
        || !std::equal(forwarded_bytes.begin(), forwarded_bytes.end(),
            payload.begin() + static_cast<std::ptrdiff_t>(forwarded_offset))) {
        throw std::runtime_error("Unexpected Millennium Atari ST forwarded MILL22A.inf JSR target");
    }
    return {load_base, entry_address, static_cast<std::uint32_t>(entry_offset),
        read_be16(payload, entry_offset), forwarded_address, static_cast<std::uint32_t>(forwarded_offset),
        read_be16(payload, forwarded_offset), read_be16(payload, forwarded_offset + 4U),
        read_be16(payload, forwarded_offset + 6U), read_be16(payload, forwarded_offset + 8U),
        read_be16(payload, forwarded_offset + 10U)};
}

MillenniumAtariConfigThirdJsr parse_millennium_atari_config_third_jsr(
    std::span<const std::uint8_t> payload, const MillenniumAtariConfigEntry& entry) {
    // Direct entry-block target $2b2be has a local 10-byte gate: the original
    // BNE.W reaches $2b300. Both the source and destination prefixes are
    // retained verbatim; their D0-dependent meanings stay unmodelled.
    constexpr std::uint32_t load_base = 0x2a4de;
    constexpr std::uint32_t target_address = 0x2b2be;
    constexpr std::size_t target_offset = target_address - load_base;
    constexpr std::uint32_t branch_target_address = 0x2b300;
    constexpr std::size_t branch_target_offset = branch_target_address - load_base;
    constexpr std::array<std::uint8_t, 10> entry_bytes{
        0x14, 0x00, 0x02, 0x00, 0x00, 0xc0, 0x66, 0x00, 0x00, 0x3a,
    };
    constexpr std::array<std::uint8_t, 8> branch_target_bytes{
        0x08, 0x02, 0x00, 0x06, 0x67, 0x00, 0x00, 0x90,
    };
    const auto has_target = std::find(entry.jsr_targets.begin(), entry.jsr_targets.end(), target_address)
        != entry.jsr_targets.end();
    if (entry.proven_load_base != load_base || !has_target
        || payload.size() < target_offset + entry_bytes.size()
        || payload.size() < branch_target_offset + branch_target_bytes.size()
        || !std::equal(entry_bytes.begin(), entry_bytes.end(),
            payload.begin() + static_cast<std::ptrdiff_t>(target_offset))
        || !std::equal(branch_target_bytes.begin(), branch_target_bytes.end(),
            payload.begin() + static_cast<std::ptrdiff_t>(branch_target_offset))) {
        throw std::runtime_error("Unexpected Millennium Atari ST third MILL22A.inf JSR target");
    }
    return {load_base, target_address, static_cast<std::uint32_t>(target_offset),
        read_be16(payload, target_offset), read_be16(payload, target_offset + 2U),
        read_be16(payload, target_offset + 4U), read_be16(payload, target_offset + 6U),
        read_be16(payload, target_offset + 8U), branch_target_address,
        read_be16(payload, branch_target_offset), read_be16(payload, branch_target_offset + 2U),
        read_be16(payload, branch_target_offset + 4U)};
}

MillenniumAtariConfigThirdRoutine parse_millennium_atari_config_third_routine(
    const std::span<const std::uint8_t> payload, const MillenniumAtariConfigEntry& entry) {
    constexpr std::uint32_t load_base = 0x2a4de;
    constexpr std::uint32_t target_address = 0x2b2be;
    constexpr std::uint32_t terminal_return_address = 0x2b3a4;
    constexpr std::size_t target_offset = target_address - load_base;
    constexpr std::size_t byte_count = terminal_return_address - target_address + 2U;
    constexpr std::array<std::uint8_t, 2> terminal_return{0x4e, 0x75};
    const auto has_target = std::find(entry.jsr_targets.begin(), entry.jsr_targets.end(), target_address)
        != entry.jsr_targets.end();
    if (entry.proven_load_base != load_base || !has_target
        || payload.size() < target_offset + byte_count
        || !std::equal(terminal_return.begin(), terminal_return.end(),
            payload.begin() + static_cast<std::ptrdiff_t>(target_offset + byte_count - terminal_return.size()))) {
        throw std::runtime_error("Unexpected Millennium Atari ST third MILL22A.inf JSR routine");
    }
    const auto bytes = payload.subspan(target_offset, byte_count);
    const auto digest = to_hex(sha256(bytes));
    if (digest != "85c58759b0cb2f067734fb006aa543fc74926422187506914c823ceaaf9c6cd8") {
        throw std::runtime_error("Unexpected Millennium Atari ST third MILL22A.inf JSR routine hash");
    }
    return {target_address, static_cast<std::uint32_t>(target_offset), terminal_return_address,
        byte_count, digest};
}

MillenniumAtariConfigFourthJsr parse_millennium_atari_config_fourth_jsr(
    std::span<const std::uint8_t> payload, const MillenniumAtariConfigEntry& entry) {
    // The direct $2b448 target starts with six unambiguous immediate/absolute
    // register writes. Stop before the following loop body, where dataflow
    // depends on memory contents and platform state outside this local proof.
    constexpr std::uint32_t load_base = 0x2a4de;
    constexpr std::uint32_t target_address = 0x2b448;
    constexpr std::size_t target_offset = target_address - load_base;
    constexpr std::array<std::uint8_t, 28> setup_bytes{
        0x3e, 0x3c, 0x00, 0x06, 0x2a, 0x7c, 0x00, 0x02, 0xb4, 0x28,
        0x28, 0x7c, 0x00, 0x02, 0xb3, 0xc8, 0x3c, 0x3c, 0x00, 0x0f,
        0x3a, 0x3c, 0x00, 0x02, 0x38, 0x3c, 0x01, 0x00,
    };
    const auto has_target = std::find(entry.jsr_targets.begin(), entry.jsr_targets.end(), target_address)
        != entry.jsr_targets.end();
    if (entry.proven_load_base != load_base || !has_target
        || payload.size() < target_offset + setup_bytes.size()
        || !std::equal(setup_bytes.begin(), setup_bytes.end(),
            payload.begin() + static_cast<std::ptrdiff_t>(target_offset))) {
        throw std::runtime_error("Unexpected Millennium Atari ST fourth MILL22A.inf JSR target");
    }
    return {load_base, target_address, static_cast<std::uint32_t>(target_offset),
        read_be16(payload, target_offset), read_be16(payload, target_offset + 2U),
        read_be32(payload, target_offset + 6U), read_be32(payload, target_offset + 12U),
        read_be16(payload, target_offset + 18U), read_be16(payload, target_offset + 22U),
        read_be16(payload, target_offset + 26U)};
}

MillenniumAtariConfigFourthPrelude parse_millennium_atari_config_fourth_prelude(
    std::span<const std::uint8_t> payload, const MillenniumAtariConfigFourthJsr& setup) {
    // The 34 literal bytes directly before $2b448 establish two DBF
    // backedges into the same local span, then fall through to the separately
    // verified setup. No callsite or dynamic entry state for $2b426 is
    // asserted here.
    constexpr std::uint32_t load_base = 0x2a4de;
    constexpr std::uint32_t prelude_address = 0x2b426;
    constexpr std::size_t prelude_offset = prelude_address - load_base;
    constexpr std::size_t prelude_bytes = 34;
    constexpr std::uint32_t continuation_address = 0x2b448;
    constexpr std::array<std::uint8_t, prelude_bytes> expected_bytes{
        0x20, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x32, 0x3c, 0x00, 0x07,
        0x2a, 0xc0, 0x51, 0xc9, 0xff, 0xfc, 0x2f, 0x0b, 0x2a, 0x7c,
        0x00, 0x02, 0xb3, 0xc8, 0x30, 0x3c, 0x00, 0x17, 0x2a, 0xdc,
        0x51, 0xc8, 0xff, 0xfc,
    };
    constexpr std::string_view expected_sha256 =
        "6f135d6e68a1b6c48826ae484223166f4e6061cd4b6b5cbc2d0dfcc2bc8fb550";
    if (setup.proven_load_base != load_base || setup.target_address != continuation_address
        || setup.target_file_offset != prelude_offset + prelude_bytes
        || payload.size() < prelude_offset + prelude_bytes) {
        throw std::runtime_error("Unexpected Millennium Atari ST fourth MILL22A.inf prelude");
    }
    const auto bytes = payload.subspan(prelude_offset, prelude_bytes);
    const auto hash = to_hex(sha256(bytes));
    if (hash != expected_sha256
        || !std::equal(expected_bytes.begin(), expected_bytes.end(), bytes.begin())) {
        throw std::runtime_error("Unexpected Millennium Atari ST fourth MILL22A.inf prelude");
    }
    return {prelude_address, static_cast<std::uint32_t>(prelude_offset),
        static_cast<std::uint32_t>(prelude_bytes), hash,
        read_be16(bytes, 0), read_be32(bytes, 2), read_be16(bytes, 6), read_be16(bytes, 8),
        read_be16(bytes, 12), static_cast<std::int16_t>(read_be16(bytes, 14)), 0x2b430,
        read_be16(bytes, 16), read_be32(bytes, 20), read_be16(bytes, 24), read_be16(bytes, 26),
        read_be16(bytes, 30), static_cast<std::int16_t>(read_be16(bytes, 32)), 0x2b442,
        continuation_address};
}

MillenniumAtariConfigFourthLoop parse_millennium_atari_config_fourth_loop(
    std::span<const std::uint8_t> payload, const MillenniumAtariConfigFourthJsr& setup) {
    // The immediate post-setup body at $2b464 ends in DBF D5,-$14. On its
    // taken path the 68000 PC base is the displacement word, returning to the
    // first byte of the same 22-byte original block. No loop iteration is
    // executed or interpreted here.
    constexpr std::uint32_t target_address = 0x2b448;
    constexpr std::uint32_t body_address = 0x2b464;
    constexpr std::size_t body_offset = body_address - 0x2a4de;
    constexpr std::array<std::uint8_t, 22> body_bytes{
        0x12, 0x14, 0x10, 0x2c, 0x00, 0x01, 0xd2, 0x00, 0x64, 0x02,
        0xd9, 0x55, 0xe8, 0x4c, 0x18, 0x81, 0x54, 0x8c, 0x51, 0xcd,
        0xff, 0xec,
    };
    constexpr std::uint16_t backedge_opcode = 0x51cd;
    constexpr std::int16_t backedge_displacement = -20;
    if (setup.target_address != target_address || setup.target_file_offset + 28U != body_offset
        || payload.size() < body_offset + body_bytes.size()
        || !std::equal(body_bytes.begin(), body_bytes.end(),
            payload.begin() + static_cast<std::ptrdiff_t>(body_offset))
        || read_be16(payload, body_offset + 18U) != backedge_opcode
        || static_cast<std::int16_t>(read_be16(payload, body_offset + 20U)) != backedge_displacement) {
        throw std::runtime_error("Unexpected Millennium Atari ST fourth MILL22A.inf loop");
    }
    return {target_address, body_address, static_cast<std::uint32_t>(body_offset),
        static_cast<std::uint32_t>(body_bytes.size()), read_be16(payload, body_offset + 18U),
        static_cast<std::int16_t>(read_be16(payload, body_offset + 20U)),
        body_address, setup.d5_initial_value};
}

MillenniumAtariConfigFourthPostLoop parse_millennium_atari_config_fourth_post_loop(
    std::span<const std::uint8_t> payload, const MillenniumAtariConfigFourthLoop& loop) {
    // Once the inner DBF falls through, the next original words are ADDQ.L
    // #2,A5; DBF D6,-$22. The taken outer backedge reaches the immediate D5
    // setup prefix already present in this same file; no loop is executed.
    constexpr std::uint32_t post_loop_address = 0x2b47a;
    constexpr std::size_t post_loop_offset = post_loop_address - 0x2a4de;
    constexpr std::uint32_t target_address = 0x2b45c;
    constexpr std::size_t target_offset = target_address - 0x2a4de;
    constexpr std::array<std::uint8_t, 6> post_loop_bytes{0x54, 0x8d, 0x51, 0xce, 0xff, 0xde};
    constexpr std::array<std::uint8_t, 4> target_bytes{0x3a, 0x3c, 0x00, 0x02};
    constexpr std::int16_t displacement = -34;
    if (loop.body_address != 0x2b464 || loop.body_file_offset + loop.body_bytes != post_loop_offset
        || payload.size() < post_loop_offset + post_loop_bytes.size()
        || payload.size() < target_offset + target_bytes.size()
        || !std::equal(post_loop_bytes.begin(), post_loop_bytes.end(),
            payload.begin() + static_cast<std::ptrdiff_t>(post_loop_offset))
        || !std::equal(target_bytes.begin(), target_bytes.end(),
            payload.begin() + static_cast<std::ptrdiff_t>(target_offset))
        || static_cast<std::int16_t>(read_be16(payload, post_loop_offset + 4U)) != displacement) {
        throw std::runtime_error("Unexpected Millennium Atari ST fourth MILL22A.inf post-loop path");
    }
    return {post_loop_address, static_cast<std::uint32_t>(post_loop_offset),
        read_be16(payload, post_loop_offset), read_be16(payload, post_loop_offset + 2U),
        static_cast<std::int16_t>(read_be16(payload, post_loop_offset + 4U)), target_address,
        read_be16(payload, target_offset), read_be16(payload, target_offset + 2U)};
}

MillenniumAtariConfigFourthOuterSetup parse_millennium_atari_config_fourth_outer_setup(
    std::span<const std::uint8_t> payload, const MillenniumAtariConfigFourthPostLoop& post_loop) {
    // The outer DBF's target at $2b45c consists only of MOVE.W #2,D5 and
    // MOVE.W #$100,D4 before directly falling through to $2b464, the verified
    // inner-loop body. This maintains the original control/dataflow boundary.
    constexpr std::uint32_t setup_address = 0x2b45c;
    constexpr std::size_t setup_offset = setup_address - 0x2a4de;
    constexpr std::uint32_t continuation_address = 0x2b464;
    constexpr std::array<std::uint8_t, 8> setup_bytes{
        0x3a, 0x3c, 0x00, 0x02, 0x38, 0x3c, 0x01, 0x00,
    };
    if (post_loop.outer_backedge_target_address != setup_address
        || payload.size() < setup_offset + setup_bytes.size()
        || !std::equal(setup_bytes.begin(), setup_bytes.end(),
            payload.begin() + static_cast<std::ptrdiff_t>(setup_offset))) {
        throw std::runtime_error("Unexpected Millennium Atari ST fourth MILL22A.inf outer-loop setup");
    }
    return {setup_address, static_cast<std::uint32_t>(setup_offset),
        read_be16(payload, setup_offset), read_be16(payload, setup_offset + 2U),
        read_be16(payload, setup_offset + 4U), read_be16(payload, setup_offset + 6U),
        continuation_address};
}

MillenniumAtariConfigFourthPostOuterBoundary parse_millennium_atari_config_fourth_post_outer_boundary(
    std::span<const std::uint8_t> payload, const MillenniumAtariConfigFourthPostLoop& post_loop) {
    // The fall-through after DBF D6 reaches a literal longword, selector $6,
    // and TRAP #14. This is the first native-service boundary after the nested
    // loops, so parsing intentionally ends at the trap opcode.
    constexpr std::uint32_t boundary_address = 0x2b480;
    constexpr std::size_t boundary_offset = boundary_address - 0x2a4de;
    constexpr std::array<std::uint8_t, 12> boundary_bytes{
        0x2f, 0x3c, 0x00, 0x02, 0xb4, 0x28, 0x3f, 0x3c, 0x00, 0x06, 0x4e, 0x4e,
    };
    if (post_loop.post_loop_address != 0x2b47a
        || payload.size() < boundary_offset + boundary_bytes.size()
        || !std::equal(boundary_bytes.begin(), boundary_bytes.end(),
            payload.begin() + static_cast<std::ptrdiff_t>(boundary_offset))) {
        throw std::runtime_error("Unexpected Millennium Atari ST fourth MILL22A.inf post-outer-loop boundary");
    }
    return {boundary_address, static_cast<std::uint32_t>(boundary_offset),
        read_be16(payload, boundary_offset), read_be32(payload, boundary_offset + 2U),
        read_be16(payload, boundary_offset + 6U), read_be16(payload, boundary_offset + 8U),
        read_be16(payload, boundary_offset + 10U)};
}

MillenniumAtariConfigFourthPostOuterTail parse_millennium_atari_config_fourth_post_outer_tail(
    std::span<const std::uint8_t> payload,
    const MillenniumAtariConfigFourthPostOuterBoundary& boundary) {
    // This immediate 26-byte suffix is retained as an exact preservation
    // anchor. Its first byte is reached only after a TRAP #14 returns, an
    // effect Project Eon deliberately does not emulate or guess.
    constexpr std::uint32_t tail_address = 0x2b48c;
    constexpr std::size_t tail_offset = tail_address - 0x2a4de;
    constexpr std::size_t tail_bytes = 26;
    constexpr std::string_view expected_sha256 =
        "34d497b9c4408944ea24d4eede21838f691c43d5a0d772db922187bed0e87fc8";
    if (boundary.boundary_address != 0x2b480 || boundary.trap_opcode != 0x4e4e
        || payload.size() < tail_offset + tail_bytes) {
        throw std::runtime_error("Unexpected Millennium Atari ST fourth MILL22A.inf post-outer-loop tail");
    }
    constexpr std::array<std::uint8_t, tail_bytes> expected_bytes{
        0x5c, 0x8f, 0x20, 0x3c, 0x00, 0x00, 0x4e, 0x20, 0x53, 0x80, 0x66, 0xfc,
        0x51, 0xcf, 0xff, 0xb2, 0x3f, 0x3c, 0x00, 0x06, 0x4e, 0x4e, 0x5c, 0x8f,
        0x4e, 0x75,
    };
    const auto bytes = payload.subspan(tail_offset, tail_bytes);
    const auto hash = to_hex(sha256(bytes));
    if (hash != expected_sha256
        || !std::equal(expected_bytes.begin(), expected_bytes.end(), bytes.begin())) {
        throw std::runtime_error("Unexpected Millennium Atari ST fourth MILL22A.inf post-outer-loop tail");
    }
    return {tail_address, static_cast<std::uint32_t>(tail_offset),
        static_cast<std::uint32_t>(tail_bytes), hash,
        read_be16(bytes, 0), read_be16(bytes, 2), read_be32(bytes, 4), read_be16(bytes, 8),
        read_be16(bytes, 10), static_cast<std::int8_t>(bytes[11]), 0x2b494,
        // DBF displacements are relative to their extension word. Therefore
        // $ffb2 at $2b49a returns to the LEA opcode at $2b44c, rather than
        // into the middle of its absolute-long operand.
        read_be16(bytes, 12), static_cast<std::int16_t>(read_be16(bytes, 14)), 0x2b44c,
        read_be16(bytes, 16), read_be16(bytes, 18), read_be16(bytes, 20),
        read_be16(bytes, 22), read_be16(bytes, 24)};
}

MillenniumAtariConfigFourthPostOuterRecurrence parse_millennium_atari_config_fourth_post_outer_recurrence(
    std::span<const std::uint8_t> payload, const MillenniumAtariConfigFourthPostOuterTail& tail,
    const MillenniumAtariConfigFourthLoop& loop) {
    // The D7 DBF returns to $2b44c, the LEA A5 opcode, then falls through the
    // A4/D6/D5/D4 literals to the separately proven loop body at $2b464.
    constexpr std::uint32_t prefix_address = 0x2b44c;
    constexpr std::size_t prefix_offset = prefix_address - 0x2a4de;
    constexpr std::size_t prefix_bytes = 24;
    constexpr std::uint32_t continuation_address = 0x2b464;
    constexpr std::array<std::uint8_t, prefix_bytes> expected_bytes{
        0x2a, 0x7c, 0x00, 0x02, 0xb4, 0x28, 0x28, 0x7c, 0x00, 0x02, 0xb3, 0xc8,
        0x3c, 0x3c, 0x00, 0x0f, 0x3a, 0x3c, 0x00, 0x02, 0x38, 0x3c, 0x01, 0x00,
    };
    constexpr std::string_view expected_sha256 =
        "85f6e69ef8d058c021e0c70fe51375ef2f09a2c67c798c73f066ffdb6f14a187";
    if (tail.d7_backedge_target_address != prefix_address
        || loop.body_address != continuation_address
        || payload.size() < prefix_offset + prefix_bytes) {
        throw std::runtime_error("Unexpected Millennium Atari ST fourth MILL22A.inf post-outer-loop recurrence");
    }
    const auto bytes = payload.subspan(prefix_offset, prefix_bytes);
    const auto hash = to_hex(sha256(bytes));
    if (hash != expected_sha256
        || !std::equal(expected_bytes.begin(), expected_bytes.end(), bytes.begin())) {
        throw std::runtime_error("Unexpected Millennium Atari ST fourth MILL22A.inf post-outer-loop recurrence");
    }
    return {prefix_address, static_cast<std::uint32_t>(prefix_offset),
        static_cast<std::uint32_t>(prefix_bytes), hash, continuation_address};
}

MillenniumAtariConfigAbsoluteJsrInventory inventory_millennium_atari_config_absolute_jsrs(
    std::span<const std::uint8_t> payload) {
    // Preserve this whole-file byte inventory for subsequent disassembly, but
    // do not promote raw patterns to executed callsites without a control-flow
    // proof. The fixed verified set makes a different config payload fail
    // closed instead of silently looking compatible.
    static constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 19> expected{{
        {0x50c, 0x2a5aa}, {0x528, 0x2a5c2}, {0x53a, 0x2b568}, {0x5a4, 0x2a51c},
        {0x5c2, 0x2b55a}, {0x5c8, 0x2aa68}, {0x5d4, 0x2aa0c}, {0x5ec, 0x2b2be},
        {0x5fe, 0x2b448}, {0x60a, 0x2aa0c}, {0xcee, 0x2a596}, {0xd08, 0x2b4c8},
        {0xd30, 0x2b2be}, {0xd42, 0x2b448}, {0xd54, 0x2b458}, {0xd7a, 0x2a596},
        {0xd94, 0x2a596}, {0xdac, 0x2b576}, {0xdb2, 0x2aa78},
    }};
    MillenniumAtariConfigAbsoluteJsrInventory result;
    for (std::size_t offset = 0; offset + 6U <= payload.size(); ++offset) {
        if (read_be16(payload, offset) == 0x4eb9U) {
            result.encodings.emplace_back(static_cast<std::uint32_t>(offset), read_be32(payload, offset + 2U));
        }
    }
    if (result.encodings.size() != expected.size()
        || !std::equal(result.encodings.begin(), result.encodings.end(), expected.begin())) {
        throw std::runtime_error("Unexpected Millennium Atari ST MILL22A.inf absolute JSR inventory");
    }
    return result;
}

MillenniumAtariConfigResidualJsrBody parse_millennium_atari_config_residual_jsr_body(
    const std::span<const std::uint8_t> payload,
    const MillenniumAtariConfigAbsoluteJsrInventory& inventory) {
    // This is the wholly static target of one inventory-only encoding. Its
    // reachability and register inputs are unrecovered, so retain the body as
    // hash-addressed bytes instead of assigning it a game or platform role.
    constexpr std::uint32_t load_base = 0x2a4de;
    constexpr std::uint32_t callsite_file_offset = 0xdac;
    constexpr std::uint32_t target_address = 0x2b576;
    constexpr std::size_t target_offset = target_address - load_base;
    constexpr std::uint32_t terminal_return_address = 0x2b5f8;
    constexpr std::size_t byte_count = terminal_return_address - target_address + 2U;
    constexpr std::string_view expected_sha256 =
        "07e36fd52b00af1557c0da08efc7388d9d7cf6567e9c24102267db80b34adcd8";
    constexpr std::array<std::uint8_t, 4> first_bytes{0x70, 0x00, 0x47, 0xfa};
    constexpr std::array<std::uint8_t, 2> return_bytes{0x4e, 0x75};
    const auto callsite = std::pair{callsite_file_offset, target_address};
    if (std::find(inventory.encodings.begin(), inventory.encodings.end(), callsite)
            == inventory.encodings.end()
        || payload.size() < target_offset + byte_count
        || !std::equal(first_bytes.begin(), first_bytes.end(),
            payload.begin() + static_cast<std::ptrdiff_t>(target_offset))
        || !std::equal(return_bytes.begin(), return_bytes.end(),
            payload.begin() + static_cast<std::ptrdiff_t>(target_offset + byte_count - return_bytes.size()))) {
        throw std::runtime_error("Unexpected Millennium Atari ST residual MILL22A.inf JSR body");
    }
    const auto bytes = payload.subspan(target_offset, byte_count);
    const auto digest = to_hex(sha256(bytes));
    if (digest != expected_sha256) {
        throw std::runtime_error("Unexpected Millennium Atari ST residual MILL22A.inf JSR body hash");
    }
    return {callsite_file_offset, target_address, static_cast<std::uint32_t>(target_offset),
        terminal_return_address, static_cast<std::uint32_t>(byte_count), digest,
        read_be16(bytes, 0), read_be16(bytes, byte_count - 2U)};
}

} // namespace eon
