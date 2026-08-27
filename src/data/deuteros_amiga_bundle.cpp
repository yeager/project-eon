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

} // namespace eon
