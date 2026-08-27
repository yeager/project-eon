#include "data/deuteros_amiga_bundle.hpp"

#include <span>
#include <stdexcept>

namespace eon {
namespace {

std::uint16_t big16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw std::runtime_error("Truncated Deuteros bundle word");
    }
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8U)
        | bytes[offset + 1]);
}

std::uint32_t big32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(big16(bytes, offset)) << 16U) | big16(bytes, offset + 2);
}

void validate_relative_offset(std::uint32_t offset, std::uint32_t length) {
    if (offset != 0 && offset >= length) {
        throw std::runtime_error("Deuteros bundle pointer outside bundle");
    }
}

} // namespace

DeuterosAmigaBundle parse_deuteros_amiga_bundle(const AmigaAdf& disk, std::uint32_t disk_offset) {
    constexpr std::size_t header_size = 60;
    const auto header = disk.bytes(disk_offset, header_size);
    DeuterosAmigaBundle bundle;
    bundle.disk_offset = disk_offset;
    bundle.length = big32(header, 0);
    bundle.object_count = big16(header, 4);
    if (bundle.length < header_size) throw std::runtime_error("Deuteros bundle shorter than header");
    // Validates that the full declared resource exists on the physical ADF.
    static_cast<void>(disk.bytes(disk_offset, bundle.length));

    std::size_t cursor = 6;
    for (auto& offset : bundle.channel_offsets) {
        offset = big32(header, cursor);
        validate_relative_offset(offset, bundle.length);
        cursor += 4;
    }
    for (auto& offset : bundle.auxiliary_offsets) {
        offset = big32(header, cursor);
        validate_relative_offset(offset, bundle.length);
        cursor += 4;
    }
    bundle.mode_flag = big16(header, cursor);
    return bundle;
}

std::array<RgbColor, 16> decode_deuteros_amiga_palette(
    const AmigaAdf& disk, const DeuterosAmigaBundle& bundle, std::uint16_t index) {
    constexpr std::uint32_t encoded_size = 16 * 2;
    const auto palette_base = bundle.auxiliary_offsets[0];
    if (palette_base == 0) throw std::runtime_error("Deuteros bundle has no palette channel");
    const auto relative = static_cast<std::uint64_t>(palette_base)
        + static_cast<std::uint64_t>(index) * encoded_size;
    if (relative > bundle.length || encoded_size > bundle.length - relative) {
        throw std::runtime_error("Deuteros palette outside bundle");
    }
    const auto encoded = disk.bytes(bundle.disk_offset + static_cast<std::size_t>(relative), encoded_size);
    std::array<RgbColor, 16> colors{};
    for (std::size_t color = 0; color < colors.size(); ++color) {
        const auto rgb4 = big16(encoded, color * 2);
        if ((rgb4 & 0xf000U) != 0) throw std::runtime_error("Invalid Amiga RGB4 colour word");
        colors[color] = {
            static_cast<std::uint8_t>(((rgb4 >> 8U) & 0xfU) * 17U),
            static_cast<std::uint8_t>(((rgb4 >> 4U) & 0xfU) * 17U),
            static_cast<std::uint8_t>((rgb4 & 0xfU) * 17U),
        };
    }
    return colors;
}

std::vector<DeuterosAmigaChannel> parse_deuteros_amiga_channels(
    const AmigaAdf& disk, const DeuterosAmigaBundle& bundle) {
    constexpr std::uint32_t header_size = 10;
    std::vector<DeuterosAmigaChannel> channels;
    channels.reserve(bundle.object_count);
    for (std::size_t index = 0; index < bundle.object_count; ++index) {
        if (index >= bundle.channel_offsets.size()) {
            throw std::runtime_error("Deuteros channel count exceeds catalogue");
        }
        const auto relative = bundle.channel_offsets[index];
        if (relative == 0 || relative > bundle.length || header_size > bundle.length - relative) {
            throw std::runtime_error("Deuteros channel header outside bundle");
        }
        const auto header = disk.bytes(bundle.disk_offset + relative, header_size);
        channels.push_back({relative, big32(header, 0), big32(header, 4),
            big16(header, 8), relative + header_size});
    }
    return channels;
}

DeuterosAmigaChannelCommand decode_deuteros_amiga_channel_command(
    const AmigaAdf& disk, const DeuterosAmigaBundle& bundle,
    std::uint32_t stream_relative_offset) {
    if (stream_relative_offset > bundle.length || 2 > bundle.length - stream_relative_offset) {
        throw std::runtime_error("Deuteros channel opcode outside bundle");
    }
    const auto opcode_bytes = disk.bytes(bundle.disk_offset + stream_relative_offset, 2);
    DeuterosAmigaChannelCommand command;
    command.opcode = big16(opcode_bytes, 0);

    // Each entry is the number and width of operands consumed after the opcode.
    // Opcodes $0-$14 are the complete range checked by the original routine.
    struct Shape { std::uint8_t count; std::array<std::uint8_t, 2> widths; };
    constexpr std::array<Shape, 0x15> shapes{{
        {0, {0, 0}}, {1, {2, 0}}, {1, {4, 0}}, {1, {2, 0}},
        {1, {2, 0}}, {1, {4, 0}}, {2, {4, 4}}, {1, {2, 0}},
        {1, {2, 0}}, {1, {4, 0}}, {1, {2, 0}}, {2, {2, 2}},
        {1, {4, 0}}, {0, {0, 0}}, {1, {2, 0}}, {1, {4, 0}},
        {0, {0, 0}}, {2, {2, 2}}, {0, {0, 0}}, {0, {0, 0}},
        {1, {2, 0}},
    }};
    if (command.opcode >= shapes.size()) throw std::runtime_error("Unknown Deuteros channel opcode");
    const auto shape = shapes[command.opcode];
    command.operand_count = shape.count;
    std::size_t encoded_size = 2;
    for (std::size_t index = 0; index < shape.count; ++index) encoded_size += shape.widths[index];
    if (encoded_size > bundle.length - stream_relative_offset) {
        throw std::runtime_error("Deuteros channel command outside bundle");
    }
    const auto encoded = disk.bytes(bundle.disk_offset + stream_relative_offset, encoded_size);
    std::size_t cursor = 2;
    for (std::size_t index = 0; index < shape.count; ++index) {
        command.operands[index] = shape.widths[index] == 2
            ? big16(encoded, cursor) : big32(encoded, cursor);
        cursor += shape.widths[index];
    }
    command.encoded_size = static_cast<std::uint8_t>(encoded_size);
    return command;
}

DeuterosAmigaIndexedBlob parse_deuteros_amiga_indexed_blob(
    const AmigaAdf& disk, const DeuterosAmigaBundle& bundle) {
    const auto table_offset = bundle.auxiliary_offsets[4];
    const auto data_offset = bundle.auxiliary_offsets[5];
    if (table_offset == 0 || data_offset <= table_offset || data_offset >= bundle.length
        || (data_offset - table_offset) % 4 != 0) {
        throw std::runtime_error("Invalid Deuteros indexed blob layout");
    }
    const auto slot_count = (data_offset - table_offset) / 4;
    const auto encoded = disk.bytes(bundle.disk_offset + table_offset,
        static_cast<std::size_t>(slot_count) * 4);
    DeuterosAmigaIndexedBlob result{table_offset, data_offset,
        bundle.length - data_offset, {}};
    result.record_offsets.reserve(slot_count);
    bool reached_unused_slots = false;
    for (std::size_t slot = 0; slot < slot_count; ++slot) {
        const auto offset = big32(encoded, slot * 4);
        // Offset zero is the genuine first record. A later zero begins the
        // unused tail, which must remain zero-filled.
        if (slot != 0 && offset == 0) reached_unused_slots = true;
        if (reached_unused_slots) {
            if (offset != 0) throw std::runtime_error("Sparse Deuteros indexed blob table");
            continue;
        }
        if (offset >= result.data_size) throw std::runtime_error("Deuteros indexed record outside blob");
        if (!result.record_offsets.empty() && offset <= result.record_offsets.back()) {
            throw std::runtime_error("Unordered Deuteros indexed blob table");
        }
        result.record_offsets.push_back(offset);
    }
    if (result.record_offsets.empty()) throw std::runtime_error("Empty Deuteros indexed blob table");
    return result;
}

} // namespace eon
