#include "data/millennium_dos_game_data.hpp"
#include "data/sha256.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace eon {
namespace {

constexpr std::size_t celestial_label_count = 41;

[[nodiscard]] std::uint16_t read_u16(std::span<const std::uint8_t> bytes,
    const std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw std::runtime_error("Truncated Millennium DOS save field");
    }
    const auto low = static_cast<std::uint16_t>(bytes[offset]);
    const auto high = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
    return static_cast<std::uint16_t>(low | high);
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
        const auto length = next - *it;
        const auto source = static_data.subspan(*it, length);
        result.records.push_back({.source_offset = *it, .source_size = length,
            .sha256 = to_hex(sha256(source))});
    }
    return result;
}

MillenniumDosF7PresentationProgram parse_millennium_dos_f7_presentation_program(
    const std::span<const std::uint8_t> game_executable,
    const std::span<const std::uint8_t> static_data) {
    constexpr std::string_view executable_hash =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    constexpr std::string_view static_data_hash =
        "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d";
    constexpr std::size_t executable_load_bias = 0x100;
    constexpr std::size_t caller_start = 0x7534;
    constexpr std::size_t caller_end = 0x7597;
    constexpr std::string_view caller_hash =
        "2f38322fe00086778bb0fea4a78460c2613f4253d44295834696f6f1a2d238ec";
    constexpr std::size_t interpreter_start = 0x05f7;
    constexpr std::size_t interpreter_end = 0x0653;
    constexpr std::string_view interpreter_hash =
        "0352f55abf23577515c3e0881df7e06fae8eba80bb4b3062aaa8db6d27a1cb56";
    constexpr std::size_t pointer_index = 0x012a;
    constexpr std::size_t expected_record_start = 0x14c4;
    constexpr std::size_t expected_record_end = 0x1525;
    constexpr std::string_view record_hash =
        "e2493bfc00889c1238d12cd0934d4ef53ffae1b2c2f040f157417217de45ecb9";
    struct FragmentProfile {
        std::size_t offset;
        std::size_t size;
        std::string_view hash;
    };
    constexpr std::array<FragmentProfile, 5> fragments{{
        {0x14c4, 21, "5bbe8d334c3bca39885298fb78c3edead3144f88136a30061b8294329c680af8"},
        {0x14d9, 25, "7637bc5611b9759455903f63f4ae92c98a785be243900c8af2bf77a95458a241"},
        {0x14f2, 17, "2cc57b6c20be32a96373636d6688c4a81977ece6dd7f4c19dabf8450f682795e"},
        {0x1503, 19, "1f401c64d45b1e8ddf8f326dc7828b26f37e08995f03882e90e2ca03a9e8afc4"},
        {0x1516, 15, "796df23d753e98ef64ab99ffefe35b1c9fd8147828d05619bc56627c6fbf9720"},
    }};
    if (game_executable.size() != 54'391 || static_data.size() != 12'494
        || to_hex(sha256(game_executable)) != executable_hash
        || to_hex(sha256(static_data)) != static_data_hash) {
        throw std::runtime_error("Unsupported Millennium DOS F7 presentation leaves");
    }
    const auto executable_span = [&](const std::size_t address, const std::size_t size) {
        if (address < executable_load_bias
            || address - executable_load_bias > game_executable.size()
            || size > game_executable.size() - (address - executable_load_bias)) {
            throw std::runtime_error("Millennium DOS F7 code span escapes its executable");
        }
        return game_executable.subspan(address - executable_load_bias, size);
    };
    if (to_hex(sha256(executable_span(caller_start, caller_end - caller_start))) != caller_hash
        || to_hex(sha256(executable_span(interpreter_start,
            interpreter_end - interpreter_start))) != interpreter_hash) {
        throw std::runtime_error("Millennium DOS F7 caller or interpreter changed");
    }

    const auto target = static_cast<std::size_t>(read_u16(static_data, pointer_index * 2));
    if (target != expected_record_start) {
        throw std::runtime_error("Millennium DOS F7 pointer no longer selects its recovered record");
    }
    std::set<std::size_t> targets;
    for (std::size_t index = 0; index < MillenniumDosStaticTextCatalog::pointer_count; ++index) {
        targets.insert(read_u16(static_data, index * 2));
    }
    const auto next = targets.upper_bound(target);
    if (next == targets.end() || *next != expected_record_end
        || expected_record_end > static_data.size()) {
        throw std::runtime_error("Millennium DOS F7 record boundary changed");
    }
    const auto record = static_data.subspan(target, expected_record_end - target);
    if (to_hex(sha256(record)) != record_hash) {
        throw std::runtime_error("Millennium DOS F7 record bytes changed");
    }

    MillenniumDosF7PresentationProgram result;
    result.executable_sha256 = std::string(executable_hash);
    result.static_data_sha256 = std::string(static_data_hash);
    result.pointer_index = pointer_index;
    result.record_offset = target;
    result.record_size = record.size();
    result.fragments.reserve(fragments.size());
    std::size_t cursor = target;
    for (const auto& fragment_profile : fragments) {
        if (cursor != fragment_profile.offset || cursor >= expected_record_end) {
            throw std::runtime_error("Millennium DOS F7 fragment order changed");
        }
        MillenniumDosPresentationFragment fragment;
        fragment.source_offset = cursor;
        const auto fragment_end = cursor + fragment_profile.size;
        if (fragment_end > expected_record_end
            || static_data[fragment_end - 1] != 0
            || to_hex(sha256(static_data.subspan(cursor, fragment_profile.size)))
                != fragment_profile.hash) {
            throw std::runtime_error("Millennium DOS F7 fragment boundary changed");
        }
        while (cursor < fragment_end - 1) {
            const auto opcode_offset = cursor;
            const auto opcode = static_data[cursor++];
            if (opcode == 0x10 || opcode == 0x11) {
                if (cursor >= fragment_end - 1) {
                    throw std::runtime_error("Truncated Millennium DOS F7 control operand");
                }
                const auto operand = static_data[cursor++];
                fragment.operations.push_back({
                    MillenniumDosPresentationOperationKind::private_interrupt_function,
                    opcode_offset, static_cast<std::uint16_t>(opcode == 0x10 ? 4 : 5), operand});
            } else if (opcode == 0x16) {
                if (cursor + 2 > fragment_end - 1) {
                    throw std::runtime_error("Truncated Millennium DOS F7 cursor operand");
                }
                fragment.operations.push_back({
                    MillenniumDosPresentationOperationKind::cursor_word, opcode_offset,
                    read_u16(static_data, cursor)});
                cursor += 2;
            } else if (opcode >= 0x20) {
                fragment.operations.push_back({
                    MillenniumDosPresentationOperationKind::glyph_code, opcode_offset, opcode});
            } else {
                throw std::runtime_error("Unsupported opcode in Millennium DOS F7 stream");
            }
        }
        if (cursor != fragment_end - 1 || static_data[cursor] != 0) {
            throw std::runtime_error("Millennium DOS F7 stream lost its terminator");
        }
        ++cursor;
        fragment.consumed_size = fragment_profile.size;
        fragment.sha256 = std::string(fragment_profile.hash);
        result.fragments.push_back(std::move(fragment));
    }
    if (cursor != expected_record_end) {
        throw std::runtime_error("Millennium DOS F7 fragment coverage is incomplete");
    }
    return result;
}

MillenniumDosStaticDataEvidence parse_millennium_dos_static_data_evidence(
    const std::span<const std::uint8_t> static_data) {
    // The generic catalog parser intentionally permits a bounded table shape
    // so it can describe each verified edition. This admission profile is
    // stricter: it makes an inspection report about known original bytes, not
    // a claim that another similarly shaped file is game data.
    constexpr std::string_view english_sha256 =
        "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d";
    constexpr std::string_view spanish_sha256 =
        "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31";
    const auto source_sha256 = to_hex(sha256(static_data));
    const bool english = static_data.size() == 12'494 && source_sha256 == english_sha256;
    const bool spanish = static_data.size() == 13'254 && source_sha256 == spanish_sha256;
    if (!english && !spanish) {
        throw std::runtime_error("Unsupported Millennium DOS static-data evidence source");
    }

    const auto game_data = parse_millennium_dos_game_data(static_data);
    const auto catalog = parse_millennium_dos_static_text_catalog(static_data);
    constexpr std::array<std::size_t, 5> anchor_indices{{
        0, 2, 251, 252, 401,
    }};
    if (game_data.celestial_table_offset != (english ? 0x03d2U : 0x03dbU)) {
        throw std::runtime_error("Unsupported Millennium DOS static-data celestial-table offset");
    }
    if (game_data.celestial_labels.size() != celestial_label_count
        || catalog.pointers.size() != MillenniumDosStaticTextCatalog::pointer_count) {
        throw std::runtime_error("Unsupported Millennium DOS static-data evidence topology");
    }
    MillenniumDosStaticDataEvidence result;
    result.source_sha256 = source_sha256;
    result.source_size = static_data.size();
    result.celestial_table_offset = game_data.celestial_table_offset;
    result.celestial_label_count = game_data.celestial_labels.size();
    result.pointer_count = catalog.pointers.size();
    result.raw_record_count = catalog.records.size();
    for (std::size_t slot = 0; slot < anchor_indices.size(); ++slot) {
        const auto index = anchor_indices[slot];
        result.topology_anchors[slot] = catalog.pointers[index];
    }
    return result;
}

MillenniumDosLaunchManual parse_millennium_dos_spanish_launch_manual(
    const std::span<const std::uint8_t> mill_bat) {
    constexpr auto expected_sha256 =
        "1fbb8246d496a6b3a35759a917ef7ae7ba36487de73104f2df81f5a1f8d9f474";
    if (mill_bat.size() != 437 || to_hex(sha256(mill_bat)) != expected_sha256) {
        throw std::runtime_error("Unsupported Millennium Spanish launch manual");
    }
    return {expected_sha256, {reinterpret_cast<const char*>(mill_bat.data()), mill_bat.size()}};
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
