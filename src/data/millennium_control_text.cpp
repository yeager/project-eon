#include "data/millennium_control_text.hpp"

#include "data/sha256.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>

namespace eon {
namespace {

[[nodiscard]] std::uint16_t read_le16(const std::span<const std::uint8_t> bytes,
    const std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2U) {
        throw std::runtime_error("Truncated Millennium DOS control-text pointer");
    }
    return static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1U]) << 8U);
}

[[nodiscard]] bool has_literal(const std::span<const std::uint8_t> bytes,
    const std::size_t offset, const std::string_view literal) {
    return offset <= bytes.size() && literal.size() <= bytes.size() - offset
        && std::equal(literal.begin(), literal.end(),
            bytes.begin() + static_cast<std::ptrdiff_t>(offset));
}

struct DosExpectedLiteral {
    std::size_t pointer_index;
    std::uint32_t record_offset;
    std::uint32_t record_size;
    std::uint32_t literal_offset;
    std::string_view literal;
    std::string_view record_sha256;
};

constexpr std::array<DosExpectedLiteral, 5> dos_literals{{
    {271, 0x12a7, 25, 0x12ac, "left button / space", "4ff26c46bfaba03c12a1a29271499c81d044ce2cccc8db06ad3e07535ad5445c"},
    {350, 0x1d88, 33, 0x1d8a, "press space bar to continue...", "ab5a128110d288c166213ef0e64b8593d1945ab8e9624363c573fe8ef942f818"},
    {390, 0x2aef, 38, 0x2af4, "press left button to continue...", "b0676d538a2ef6b07cdf467bb10a4dbea34af96fccafc90180a01825935c1d4f"},
    {398, 0x2bcd, 22, 0x2bd6, "MOUSE MODE", "220c3cd2cb86c2353f8f9320e6ec7c469007e4bd31e11dce52c847f8c510c5cc"},
    {399, 0x2be3, 22, 0x2bea, "KEYBOARD MODE", "0951952248daef3634e418d0bed0cfa2ea8cd58f7975ee5e77880c54ad731f2d"},
}};

struct AtariExpectedLiteral {
    std::uint32_t literal_offset;
    std::string_view literal;
};

constexpr std::array<AtariExpectedLiteral, 5> atari_literals{{
    {0x12425, "SAVE GAME"},
    {0x12436, "LOAD GAME"},
    {0x12445, "press left button to continue..."},
    {0x1255d, "MOUSE MODE"},
    {0x12572, "KEYBOARD MODE"},
}};

} // namespace

MillenniumDosControlTextEvidence parse_millennium_dos_control_text_evidence(
    const std::span<const std::uint8_t> static_data) {
    constexpr std::string_view source_sha256 = "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d";
    constexpr std::size_t pointer_table_size = 0x366;
    if (static_data.size() < pointer_table_size || to_hex(sha256(static_data)) != source_sha256) {
        throw std::runtime_error("Unsupported Millennium DOS control-text source");
    }
    MillenniumDosControlTextEvidence result;
    result.source_sha256 = std::string(source_sha256);
    for (std::size_t slot = 0; slot < dos_literals.size(); ++slot) {
        const auto& expected = dos_literals[slot];
        const auto target = read_le16(static_data, expected.pointer_index * 2U);
        if (target != expected.record_offset || expected.record_offset > static_data.size()
            || expected.record_size > static_data.size() - expected.record_offset
            || !has_literal(static_data, expected.literal_offset, expected.literal)
            || to_hex(sha256(static_data.subspan(expected.record_offset, expected.record_size))) != expected.record_sha256) {
            throw std::runtime_error("Unsupported Millennium DOS control-text record");
        }
        result.pointer_indices[slot] = expected.pointer_index;
        result.literals[slot] = {expected.record_offset, expected.record_size,
            expected.literal_offset, std::string(expected.literal), std::string(expected.record_sha256)};
    }
    return result;
}

MillenniumAtariPhysicalControlTextEvidence parse_millennium_atari_physical_control_text_evidence(
    const std::span<const std::uint8_t> physical_dump) {
    constexpr std::string_view source_sha256 = "081d8bc102b8c7669c5cb21abace9b08532bc0b34164f11465d0c87b63a422fd";
    constexpr std::uint32_t span_offset = 0x12420;
    constexpr std::uint32_t span_size = 368;
    constexpr std::string_view span_sha256 = "6330b762858bb4b1fb0bc17f4f577eca3b1e8de4c078fd3fc01192bcd05a89f7";
    if (to_hex(sha256(physical_dump)) != source_sha256 || span_offset > physical_dump.size()
        || span_size > physical_dump.size() - span_offset
        || to_hex(sha256(physical_dump.subspan(span_offset, span_size))) != span_sha256) {
        throw std::runtime_error("Unsupported Millennium Atari physical control-text source");
    }
    MillenniumAtariPhysicalControlTextEvidence result;
    result.source_sha256 = std::string(source_sha256);
    result.span_offset = span_offset;
    result.span_size = span_size;
    result.span_sha256 = std::string(span_sha256);
    for (std::size_t slot = 0; slot < atari_literals.size(); ++slot) {
        const auto& expected = atari_literals[slot];
        if (!has_literal(physical_dump, expected.literal_offset, expected.literal)) {
            throw std::runtime_error("Unsupported Millennium Atari physical control-text literal");
        }
        result.literals[slot] = {span_offset, span_size, expected.literal_offset,
            std::string(expected.literal), std::string(span_sha256)};
    }
    return result;
}

} // namespace eon
