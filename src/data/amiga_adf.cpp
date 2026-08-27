#include "data/amiga_adf.hpp"

#include <stdexcept>

namespace eon {
namespace {

std::uint32_t big32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) throw std::runtime_error("Truncated Amiga field");
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U)
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 16U)
        | (static_cast<std::uint32_t>(bytes[offset + 2]) << 8U)
        | static_cast<std::uint32_t>(bytes[offset + 3]);
}

} // namespace

AmigaAdf::AmigaAdf(std::vector<std::uint8_t> image) : image_(std::move(image)) {
    if (image_.size() != standard_size) throw std::runtime_error("ADF must be a standard 880 KiB image");
    if (image_[0] == 'D' && image_[1] == 'O' && image_[2] == 'S') kind_ = AmigaDiskKind::dos;
    else if (image_[0] == 'D' && image_[1] == 'E' && image_[2] == 'U') kind_ = AmigaDiskKind::deuteros_data;
}

std::string AmigaAdf::identifier() const {
    return std::string(image_.begin(), image_.begin() + 4);
}

bool AmigaAdf::boot_checksum_valid() const {
    std::uint32_t sum = 0;
    for (std::size_t offset = 0; offset < 1024; offset += 4) {
        const auto value = big32(image_, offset);
        const auto previous = sum;
        sum += value;
        if (sum < previous) ++sum;
    }
    return sum == 0xffffffffU;
}

std::uint32_t AmigaAdf::root_block() const {
    return big32(image_, 8);
}

std::span<const std::uint8_t, AmigaAdf::sector_size> AmigaAdf::sector(
    unsigned cylinder, unsigned side, unsigned sector_index) const {
    if (cylinder >= cylinders || side >= sides || sector_index >= sectors_per_track) {
        throw std::out_of_range("ADF sector coordinates outside disk");
    }
    const auto logical = (static_cast<std::size_t>(cylinder) * sides + side)
        * sectors_per_track + sector_index;
    return std::span<const std::uint8_t, sector_size>(image_.data() + logical * sector_size, sector_size);
}

std::span<const std::uint8_t, 1024> AmigaAdf::boot_block() const {
    return std::span<const std::uint8_t, 1024>(image_.data(), 1024);
}

} // namespace eon
