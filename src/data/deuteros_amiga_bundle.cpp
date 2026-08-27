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

} // namespace eon
