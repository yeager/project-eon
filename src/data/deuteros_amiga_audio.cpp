#include "data/deuteros_amiga_audio.hpp"

#include "data/sha256.hpp"

#include <span>
#include <stdexcept>

namespace eon {
namespace {

std::uint16_t big16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw std::runtime_error("Truncated Deuteros sound-table word");
    }
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8U)
        | bytes[offset + 1]);
}

std::uint32_t big32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(big16(bytes, offset)) << 16U) | big16(bytes, offset + 2);
}

} // namespace

DeuterosAmigaSoundBank parse_deuteros_amiga_sound_bank(
    const AmigaAdf& disk, const DeuterosAmigaBundle& bundle) {
    constexpr std::uint32_t entry_size = 14;
    // $212ca consumes auxiliary pointer 3, and $212d6 consumes pointer 4.
    // $22ab8 indexes the former in 14-byte units, so the latter is its only
    // evidence-backed boundary.
    const auto table = bundle.auxiliary_offsets[3];
    const auto end = bundle.auxiliary_offsets[4];
    if (table == 0 || end <= table || end > bundle.length) {
        throw std::runtime_error("Invalid Deuteros sound-table layout");
    }

    const auto encoded = disk.bytes(bundle.disk_offset + table, end - table);
    const auto table_size = (encoded.size() / entry_size) * entry_size;
    if (table_size == 0) throw std::runtime_error("Empty Deuteros sound-table layout");
    // The physical tail cannot be another entry because $22ab8 strides by
    // exactly 14 bytes. Preserve its source identity: bundle 0 has four
    // non-zero bytes here, but no recovered code reaches them as a record.
    DeuterosAmigaSoundBank result{table, static_cast<std::uint32_t>(encoded.size()),
        to_hex(sha256(encoded)), {}, {static_cast<std::uint32_t>(table + table_size),
            static_cast<std::uint32_t>(encoded.size() - table_size),
            to_hex(sha256(encoded.subspan(table_size)))}};
    result.sounds.reserve(table_size / entry_size);
    for (std::size_t offset = 0; offset < table_size; offset += entry_size) {
        const auto sample_relative_offset = big32(encoded, offset);
        const auto length_words = big16(encoded, offset + 4);
        const auto period = big16(encoded, offset + 6);
        const auto volume = big16(encoded, offset + 8);
        const auto control_word = big16(encoded, offset + 10);
        const auto parameter_word = big16(encoded, offset + 12);
        if (length_words == 0 || period == 0 || volume > 64) {
            throw std::runtime_error("Invalid Deuteros Paula sound entry");
        }
        // `length_words` is an on-media 16-bit word count. Widen before the
        // byte conversion; its maximum (131070 bytes) cannot overflow the
        // 32-bit bundle-relative bounds checked below.
        const auto byte_length = static_cast<std::uint32_t>(length_words) * 2U;
        if (sample_relative_offset > bundle.length || byte_length > bundle.length - sample_relative_offset) {
            throw std::runtime_error("Deuteros Paula sample outside bundle");
        }
        const auto sample = disk.bytes(bundle.disk_offset + sample_relative_offset, byte_length);
        result.sounds.push_back({static_cast<std::uint32_t>(offset), sample_relative_offset,
            length_words, period, volume, control_word, parameter_word,
            to_hex(sha256(encoded.subspan(offset, entry_size))), to_hex(sha256(sample)),
            sample});
    }
    return result;
}

} // namespace eon
