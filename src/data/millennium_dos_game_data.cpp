#include "data/millennium_dos_game_data.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <stdexcept>

namespace eon {
namespace {

constexpr std::size_t celestial_label_count = 41;

[[nodiscard]] std::uint16_t read_u16(std::span<const std::uint8_t> bytes,
    const std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw std::runtime_error("Truncated Millennium DOS save field");
    }
    return static_cast<std::uint16_t>(bytes[offset])
        | (static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
}

} // namespace

MillenniumDosGameData parse_millennium_dos_game_data(
    std::span<const std::uint8_t> static_data) {
    // The verified English and Spanish binaries place the same 41-entry
    // display table at different file offsets. These are format evidence,
    // not translated replacement strings: every returned byte comes from the
    // caller's original media (including spaces and original spelling).
    constexpr std::array<std::size_t, 2> table_offsets{0x3d2, 0x3db};
    for (const auto table_offset : table_offsets) {
        if (table_offset >= static_data.size()) continue;
        MillenniumDosGameData result;
        result.celestial_table_offset = table_offset;
        result.celestial_labels.reserve(celestial_label_count);
        std::size_t offset = table_offset;
        for (std::size_t index = 0; index < celestial_label_count; ++index) {
            if (offset >= static_data.size()) break;
            const auto terminator = std::find(static_data.begin() + static_cast<std::ptrdiff_t>(offset),
                static_data.end(), std::uint8_t{0});
            if (terminator == static_data.end() || terminator == static_data.begin()
                + static_cast<std::ptrdiff_t>(offset)) break;
            const auto length = static_cast<std::size_t>(terminator
                - (static_data.begin() + static_cast<std::ptrdiff_t>(offset)));
            const std::string value(reinterpret_cast<const char*>(static_data.data() + offset), length);
            result.celestial_labels.push_back({.source_offset = offset, .text = value});
            offset += length + 1;
        }
        if (result.celestial_labels.size() == celestial_label_count) return result;
    }
    throw std::runtime_error("Unsupported Millennium DOS celestial-label table");
}

MillenniumDosStaticTextCatalog parse_millennium_dos_static_text_catalog(
    const std::span<const std::uint8_t> static_data) {
    if (static_data.size() <= MillenniumDosStaticTextCatalog::pointer_table_size) {
        throw std::runtime_error("Truncated Millennium DOS static text catalog");
    }

    MillenniumDosStaticTextCatalog result;
    result.pointers.reserve(MillenniumDosStaticTextCatalog::pointer_count);
    std::set<std::size_t> record_offsets;
    for (std::size_t index = 0; index < MillenniumDosStaticTextCatalog::pointer_count; ++index) {
        const auto target_offset = static_cast<std::size_t>(read_u16(static_data, index * 2));
        if (target_offset < MillenniumDosStaticTextCatalog::pointer_table_size
            || target_offset >= static_data.size()) {
            throw std::runtime_error("Invalid Millennium DOS static text pointer");
        }
        result.pointers.push_back({.table_index = index, .target_offset = target_offset});
        record_offsets.insert(target_offset);
    }
    // Both verified DOS editions begin at the first byte after the pointer
    // table. Requiring this catches shifted or invented tables while still
    // allowing each edition's differently sized translated records.
    if (result.pointers.front().target_offset != MillenniumDosStaticTextCatalog::pointer_table_size) {
        throw std::runtime_error("Unsupported Millennium DOS static text table origin");
    }

    result.records.reserve(record_offsets.size());
    for (auto it = record_offsets.begin(); it != record_offsets.end(); ++it) {
        const auto next = std::next(it) == record_offsets.end() ? static_data.size() : *std::next(it);
        if (next <= *it) throw std::runtime_error("Invalid Millennium DOS static text record range");
        result.records.push_back({.source_offset = *it,
            .bytes = {static_data.begin() + static_cast<std::ptrdiff_t>(*it),
                static_data.begin() + static_cast<std::ptrdiff_t>(next)}});
    }
    return result;
}

MillenniumDosSaveLayout parse_millennium_dos_save_layout(
    const std::span<const std::uint8_t> save_data) {
    // `2200AD.EXE` reads this file in fixed chunks.  Its `$c7fe` load path
    // first reads and compares the two-byte version against its `$2fb0`
    // constant ($0056), then reads exactly 0x2542 bytes.  Accept neither a
    // shorter prefix nor trailing bytes: this parser is a preservation reader,
    // not a permissive replacement format.
    if (save_data.size() != MillenniumDosSaveLayout::serialized_size) {
        throw std::runtime_error("Unsupported Millennium DOS save size");
    }

    MillenniumDosSaveLayout result;
    result.version = read_u16(save_data, MillenniumDosSaveLayout::version_offset);
    if (result.version != MillenniumDosSaveLayout::expected_version) {
        throw std::runtime_error("Unsupported Millennium DOS save version");
    }

    // At `$c87c`, the original loop executes 38 `lodsw`, writing each word to
    // `runtime_base + index * 0x1c + 6`.  The source buffer is 0x80 bytes;
    // its remaining 52 bytes belong to other native tables and are not
    // reinterpreted here.
    for (std::size_t index = 0; index < MillenniumDosSaveLayout::state_table_count;
         ++index) {
        const auto offset = MillenniumDosSaveLayout::state_table_offset_6_column + index * 2;
        result.state_table[index].runtime_offset_6 = read_u16(save_data, offset);
    }

    // At `$c8f9`, 38 source pairs are restored to offsets +0 and +4 in the
    // same 0x1c-stride table.  `$c913` then restores 38 words to offset +8.
    for (std::size_t index = 0; index < MillenniumDosSaveLayout::state_table_count;
         ++index) {
        const auto pair_offset = MillenniumDosSaveLayout::state_table_offset_0_and_4_columns
            + index * 4;
        result.state_table[index].runtime_offset_0 = read_u16(save_data, pair_offset);
        result.state_table[index].runtime_offset_4 = read_u16(save_data, pair_offset + 2);
        result.state_table[index].runtime_offset_8 = read_u16(save_data,
            MillenniumDosSaveLayout::state_table_offset_8_column + index * 2);
    }
    return result;
}

} // namespace eon
