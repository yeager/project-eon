#pragma once

#include "data/amiga_adf.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace eon {

struct DeuterosAmigaBundle {
    std::uint32_t disk_offset = 0;
    std::uint32_t length = 0;
    std::uint16_t object_count = 0;
    std::array<std::uint32_t, 7> channel_offsets{};
    std::array<std::uint32_t, 6> auxiliary_offsets{};
    std::uint16_t mode_flag = 0;
};

struct RgbColor {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;

    bool operator==(const RgbColor&) const = default;
};

struct DeuterosAmigaChannel {
    std::uint32_t relative_offset = 0;
    std::uint32_t initial_state_0 = 0;
    std::uint32_t initial_state_4 = 0;
    std::uint16_t initial_state_8 = 0;
    std::uint32_t stream_relative_offset = 0;
};

struct DeuterosAmigaChannelCommand {
    std::uint16_t opcode = 0;
    std::array<std::uint32_t, 2> operands{};
    std::uint8_t operand_count = 0;
    std::uint8_t encoded_size = 0;
};

struct DeuterosAmigaIndexedBlob {
    std::uint32_t table_relative_offset = 0;
    std::uint32_t data_relative_offset = 0;
    std::uint32_t data_size = 0;
    std::vector<std::uint32_t> record_offsets;
};

// Parse the in-memory pointer catalogue used by the original 68000 program.
// Offsets remain relative to the bundle, just as they are stored on disk.
[[nodiscard]] DeuterosAmigaBundle parse_deuteros_amiga_bundle(
    const AmigaAdf& disk, std::uint32_t disk_offset);

// The first auxiliary channel is indexed in 32-byte steps by the original
// code and copied as sixteen Amiga RGB4 colour words.
[[nodiscard]] std::array<RgbColor, 16> decode_deuteros_amiga_palette(
    const AmigaAdf& disk, const DeuterosAmigaBundle& bundle, std::uint16_t index);

[[nodiscard]] std::vector<DeuterosAmigaChannel> parse_deuteros_amiga_channels(
    const AmigaAdf& disk, const DeuterosAmigaBundle& bundle);

// Decode one command using the exact operand widths selected by the original
// interpreter at $214aa. Control-flow execution is intentionally separate.
[[nodiscard]] DeuterosAmigaChannelCommand decode_deuteros_amiga_channel_command(
    const AmigaAdf& disk, const DeuterosAmigaBundle& bundle,
    std::uint32_t stream_relative_offset);

// Auxiliary slots 4 and 5 delimit a big-endian offset table and its indexed
// payload. Its content semantics are deliberately left unnamed until proven.
[[nodiscard]] DeuterosAmigaIndexedBlob parse_deuteros_amiga_indexed_blob(
    const AmigaAdf& disk, const DeuterosAmigaBundle& bundle);

} // namespace eon
