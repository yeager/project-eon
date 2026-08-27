#include "data/deuteros_atari_boot.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace eon {
namespace {

std::uint16_t be16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw std::runtime_error("Truncated Deuteros Atari ST first-stage word");
    }
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8U)
        | bytes[offset + 1]);
}

std::uint32_t be32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw std::runtime_error("Truncated Deuteros Atari ST first-stage longword");
    }
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U)
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 16U)
        | (static_cast<std::uint32_t>(bytes[offset + 2]) << 8U) | bytes[offset + 3];
}

void require_bytes(std::span<const std::uint8_t> bytes, std::size_t offset,
    std::span<const std::uint8_t> expected, const char* what) {
    if (offset > bytes.size() || expected.size() > bytes.size() - offset
        || !std::equal(expected.begin(), expected.end(),
            bytes.begin() + static_cast<std::ptrdiff_t>(offset))) {
        throw std::runtime_error(what);
    }
}

} // namespace

DeuterosAtariFirstStageProfile parse_deuteros_atari_first_stage(
    std::span<const std::uint8_t> bytes) {
    if (bytes.size() != 0x1200U || be16(bytes, 0) != 0x6000U) {
        throw std::runtime_error("Unsupported Deuteros Atari ST first-stage header");
    }
    // 68000 word branch displacement is relative to the extension word's
    // address. The boot stage jumps directly to $09c4.
    const auto entry_offset = static_cast<std::size_t>(2U + be16(bytes, 2));
    if (entry_offset != 0x9c4U || entry_offset >= bytes.size()) {
        throw std::runtime_error("Unexpected Deuteros Atari ST first-stage entry");
    }
    // The checksum loop scans $43c literal stage bytes from offset $6, with
    // add-byte/rotate-left-8 and this seed/comparison pair.
    constexpr std::array<std::uint8_t, 16> checksum_setup{{
        0x41, 0xfa, 0xf5, 0xe4, // lea $10006(pc),a0
        0x20, 0x3c, 0x00, 0x00, 0x04, 0x3b,
        0x22, 0x3c, 0x22, 0x22, 0x55, 0x55}};
    require_bytes(bytes, 0xa20, checksum_setup, "Unexpected Deuteros Atari ST checksum setup");
    constexpr std::array<std::uint8_t, 6> checksum_compare{{0x0c, 0x81, 0x7a, 0xe2, 0x6a, 0xf7}};
    require_bytes(bytes, 0xa3a, checksum_compare, "Unexpected Deuteros Atari ST checksum comparison");
    // After validation, the stage starts another literal Floprd call. Its
    // preceding register setup fixes physical track 2 and RAM $70000.
    constexpr std::array<std::uint8_t, 14> next_stage_setup{{
        0x7a, 0x00, 0x7c, 0x02, 0x7e, 0x00,
        0x4d, 0xf9, 0x00, 0x07, 0x00, 0x00,
        0x48, 0x56}};
    require_bytes(bytes, 0xa68, next_stage_setup, "Unexpected Deuteros Atari ST next-stage setup");
    constexpr std::array<std::uint8_t, 26> next_stage_floprd{{
        0x3f, 0x3c, 0x00, 0x09, 0x3f, 0x07, 0x3f, 0x06,
        0x3f, 0x3c, 0x00, 0x01, 0x42, 0x67, 0x42, 0xa7,
        0x48, 0x56, 0x3f, 0x3c, 0x00, 0x08, 0x4e, 0x4e,
        0x4f, 0xef}};
    require_bytes(bytes, 0xa9c, next_stage_floprd, "Unexpected Deuteros Atari ST next-stage Floprd");
    constexpr std::array<std::uint8_t, 20> copy_setup{{
        0x20, 0x5f, 0x43, 0xf8, 0x1e, 0x00,
        0x2f, 0x09, 0x20, 0x3c, 0x00, 0x00, 0x11, 0xff,
        0x12, 0xd8, 0x51, 0xc8, 0xff, 0xfc}};
    require_bytes(bytes, 0xac8, copy_setup, "Unexpected Deuteros Atari ST first-stage copy");

    return {.entry_offset = entry_offset,
        .checksum_start_offset = 6,
        .checksum_byte_count = static_cast<std::size_t>(be32(bytes, 0xa26)) + 1U,
        .checksum_seed = be32(bytes, 0xa2c),
        .checksum_expected = be32(bytes, 0xa3c),
        .next_track = static_cast<std::uint16_t>(be16(bytes, 0xa6a) & 0x00ffU),
        .next_side = 0,
        .next_sector = 1,
        .next_sector_count = be16(bytes, 0xa9e),
        .next_destination = be32(bytes, 0xa70),
        .copy_destination = be16(bytes, 0xacc),
        .copy_byte_count = static_cast<std::size_t>(be32(bytes, 0xad2)) + 1U};
}

} // namespace eon
