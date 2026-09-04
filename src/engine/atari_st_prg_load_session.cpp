#include "engine/atari_st_prg_load_session.hpp"

#include "data/sha256.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace eon {
namespace {

constexpr std::size_t prg_header_bytes = 28;

std::uint32_t read_be32(const std::span<const std::uint8_t> bytes,
    const std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw std::runtime_error("Truncated Atari ST PRG relocation word");
    }
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U)
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 16U)
        | (static_cast<std::uint32_t>(bytes[offset + 2]) << 8U)
        | bytes[offset + 3];
}

void write_be32(const std::span<std::uint8_t> bytes, const std::size_t offset,
    const std::uint32_t value) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw std::runtime_error("Atari ST PRG relocation write outside image");
    }
    bytes[offset] = static_cast<std::uint8_t>(value >> 24U);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 16U);
    bytes[offset + 2] = static_cast<std::uint8_t>(value >> 8U);
    bytes[offset + 3] = static_cast<std::uint8_t>(value);
}

} // namespace

AtariStPrgLoadCheckpoint materialize_atari_st_prg_load(
    const std::span<const std::uint8_t> bytes, const AtariStPrg& prg,
    const std::uint32_t load_base, const std::uint32_t address_limit_exclusive) {
    // Reparse at the consumption boundary. A caller cannot pair bytes with a
    // stale or fabricated relocation description.
    const auto verified = parse_atari_st_prg(bytes);
    if (verified.text_bytes != prg.text_bytes || verified.data_bytes != prg.data_bytes
        || verified.bss_bytes != prg.bss_bytes || verified.symbol_bytes != prg.symbol_bytes
        || verified.flags != prg.flags || verified.absolute_flag != prg.absolute_flag
        || verified.relocations != prg.relocations) {
        throw std::runtime_error("Atari ST PRG metadata changed before native loading");
    }

    const auto loadable_size64 = static_cast<std::uint64_t>(prg.text_bytes) + prg.data_bytes;
    const auto image_size64 = loadable_size64 + prg.bss_bytes;
    if (address_limit_exclusive == 0 || image_size64 > std::numeric_limits<std::size_t>::max()
        || load_base >= address_limit_exclusive
        || image_size64 > static_cast<std::uint64_t>(address_limit_exclusive - load_base)
        || prg_header_bytes > bytes.size()
        || loadable_size64 > bytes.size() - prg_header_bytes) {
        throw std::runtime_error("Atari ST PRG native image lies outside its address space");
    }

    AtariStPrgLoadCheckpoint result;
    result.load_base = load_base;
    result.entry_address = load_base;
    result.text_bytes = prg.text_bytes;
    result.data_bytes = prg.data_bytes;
    result.bss_bytes = prg.bss_bytes;
    result.source_sha256 = to_hex(sha256(bytes));
    const auto source = bytes.subspan(prg_header_bytes,
        static_cast<std::size_t>(loadable_size64));
    result.loadable_source_sha256 = to_hex(sha256(source));
    result.image.resize(static_cast<std::size_t>(image_size64), 0);
    std::copy(source.begin(), source.end(), result.image.begin());
    result.relocation_effects.reserve(prg.relocations.size());

    for (const auto& relocation : prg.relocations) {
        if (relocation.offset > result.image.size()
            || result.image.size() - relocation.offset < 4
            || relocation.offset > std::numeric_limits<std::uint32_t>::max() - load_base
            || relocation.original_value > std::numeric_limits<std::uint32_t>::max() - load_base) {
            throw std::runtime_error("Atari ST PRG relocation overflows native address space");
        }
        const auto current = read_be32(result.image, relocation.offset);
        if (current != relocation.original_value) {
            throw std::runtime_error("Atari ST PRG relocation source changed before native loading");
        }
        const auto relocated = relocation.original_value + load_base;
        if (relocated >= address_limit_exclusive) {
            throw std::runtime_error("Atari ST PRG relocated value lies outside native address space");
        }
        write_be32(result.image, relocation.offset, relocated);
        result.relocation_effects.push_back({relocation.offset,
            load_base + relocation.offset, relocation.original_value, relocated});
    }
    result.materialized_image_sha256 = to_hex(sha256(result.image));
    return result;
}

NativeRuntimeEffectBatch make_atari_st_prg_load_effect_batch(
    const AtariStPrgLoadCheckpoint& checkpoint, std::string id) {
    if (id.empty() || checkpoint.image.empty() || checkpoint.load_base == 0
        || checkpoint.entry_address != checkpoint.load_base
        || checkpoint.image.size()
            != static_cast<std::uint64_t>(checkpoint.text_bytes)
                + checkpoint.data_bytes + checkpoint.bss_bytes
        || checkpoint.materialized_image_sha256 != to_hex(sha256(checkpoint.image))) {
        throw std::runtime_error("Unadmitted Atari ST PRG image effect batch");
    }
    NativeRuntimeEffectBatch batch{std::move(id), true, {}};
    batch.effects.reserve(checkpoint.image.size());
    for (std::size_t index = 0; index < checkpoint.image.size(); ++index) {
        batch.effects.push_back({index + 1,
            {NativeRuntimeAddressSpace::linear, std::nullopt,
                static_cast<std::uint64_t>(checkpoint.load_base) + index},
            MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian,
            checkpoint.image[index]});
    }
    return batch;
}

AtariStPrgLoadDiagnostics atari_st_prg_load_diagnostics(
    const AtariStPrgLoadCheckpoint& checkpoint) {
    if (checkpoint.image.empty() || checkpoint.relocation_effects.empty()
        || checkpoint.materialized_image_sha256 != to_hex(sha256(checkpoint.image))) {
        throw std::runtime_error("Unadmitted Atari ST PRG diagnostics checkpoint");
    }
    return {checkpoint.load_base, checkpoint.entry_address, checkpoint.image.size(),
        checkpoint.relocation_effects.size(), checkpoint.source_sha256,
        checkpoint.materialized_image_sha256, checkpoint.relocation_effects.front(),
        checkpoint.relocation_effects.back()};
}

MillenniumAtariPrgLoadSession::MillenniumAtariPrgLoadSession(
    const std::span<const std::uint8_t> program) {
    constexpr std::string_view expected_sha256 =
        "4584ddc459e3bf03e642f3156fbedb74aa33a847db4937beb5635eb492e93686";
    if (to_hex(sha256(program)) != expected_sha256) {
        throw std::runtime_error("Unsupported Millennium Atari ST PRG image");
    }
    const auto prg = parse_atari_st_prg(program);
    checkpoint_ = materialize_atari_st_prg_load(program, prg, native_load_base);
    if (checkpoint_.source_sha256 != expected_sha256
        || checkpoint_.loadable_source_sha256
            != "57017c09dd58c608d713fa3ad44af48ef1e07c1ac90caf303e6f17179719b3c0"
        || checkpoint_.materialized_image_sha256
            != "92eac35edb2b5db721dd5353cfc3260dfb5fb4120026b76788659aaa342f887c"
        || checkpoint_.text_bytes != 4446U || checkpoint_.data_bytes != 44564U
        || checkpoint_.bss_bytes != 81382U || checkpoint_.image.size() != 130392U
        || checkpoint_.relocation_effects.size() != 227U
        || checkpoint_.relocation_effects.front().image_offset != 0x6U
        || checkpoint_.relocation_effects.front().source_value != 0x115eU
        || checkpoint_.relocation_effects.front().relocated_value != 0x1115eU
        || checkpoint_.relocation_effects.back().image_offset != 0x1150U
        || checkpoint_.relocation_effects.back().source_value != 0x139c8U
        || checkpoint_.relocation_effects.back().relocated_value != 0x239c8U) {
        throw std::runtime_error("Unexpected Millennium Atari ST native PRG load result");
    }
}

} // namespace eon
