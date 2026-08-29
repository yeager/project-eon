#include "data/atari_st_stx.hpp"

#include <algorithm>
#include <stdexcept>

namespace eon {
namespace {

std::uint16_t little16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw std::runtime_error("Truncated STX field");
    }
    return static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(bytes[offset + 1]) << 8U;
}

std::uint32_t little32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw std::runtime_error("Truncated STX field");
    }
    return static_cast<std::uint32_t>(bytes[offset])
        | static_cast<std::uint32_t>(bytes[offset + 1]) << 8U
        | static_cast<std::uint32_t>(bytes[offset + 2]) << 16U
        | static_cast<std::uint32_t>(bytes[offset + 3]) << 24U;
}

} // namespace

AtariStStxPhysicalDisk::AtariStStxPhysicalDisk(std::vector<std::uint8_t> image)
    : image_(std::move(image)) {
    constexpr std::size_t file_header_size = 16;
    constexpr std::size_t track_header_size = 16;
    constexpr std::size_t sector_descriptor_size = 16;
    if (image_.size() < file_header_size || image_[0] != 'R' || image_[1] != 'S'
        || image_[2] != 'Y' || image_[3] != 0 || little16(image_, 4) != 3) {
        throw std::runtime_error("Unsupported STX header");
    }
    const auto declared_tracks = image_[10];
    std::size_t offset = file_header_size;
    while (offset < image_.size()) {
        if (image_.size() - offset < track_header_size) {
            throw std::runtime_error("Truncated STX track header");
        }
        const auto block_size = static_cast<std::size_t>(little32(image_, offset));
        const auto fuzzy_size = static_cast<std::size_t>(little32(image_, offset + 4));
        const auto sector_count = static_cast<std::size_t>(little16(image_, offset + 8));
        if (block_size < track_header_size || block_size > image_.size() - offset
            || sector_count > (block_size - track_header_size) / sector_descriptor_size) {
            throw std::runtime_error("Invalid STX track extent");
        }
        const auto descriptors_end = offset + track_header_size + sector_count * sector_descriptor_size;
        if (fuzzy_size > offset + block_size - descriptors_end) {
            throw std::runtime_error("STX fuzzy-data extent outside track");
        }
        const auto payload_base = descriptors_end + fuzzy_size;
        const auto track_number = image_[offset + 14];
        for (std::size_t index = 0; index < sector_count; ++index) {
            const auto descriptor = offset + track_header_size + index * sector_descriptor_size;
            const auto data_offset = static_cast<std::size_t>(little32(image_, descriptor));
            const auto size_code = image_[descriptor + 11];
            if (size_code > 7) throw std::runtime_error("Unsupported STX sector size code");
            const auto payload_size = static_cast<std::size_t>(128U) << size_code;
            if (data_offset > offset + block_size - payload_base
                || payload_size > offset + block_size - payload_base - data_offset) {
                throw std::runtime_error("STX sector payload outside track");
            }
            const auto track = image_[descriptor + 8];
            const auto side = image_[descriptor + 9];
            if (track != (track_number & 0x7fU) || side != (track_number >> 7U)) {
                throw std::runtime_error("Unsupported STX sector identity");
            }
            const auto id = image_[descriptor + 10];
            const auto duplicate = std::find_if(sectors_.begin(), sectors_.end(),
                [track, side, id](const AtariStStxSector& sector) {
                    return sector.track == track && sector.side == side && sector.id == id;
                });
            if (duplicate != sectors_.end()) throw std::runtime_error("Duplicate STX sector identity");
            sectors_.push_back({track, side, id, size_code, image_[descriptor + 14],
                payload_base + data_offset, payload_size});
        }
        ++track_count_;
        offset += block_size;
    }
    if (offset != image_.size() || track_count_ != declared_tracks) {
        throw std::runtime_error("STX track count does not match container");
    }
}

std::span<const std::uint8_t> AtariStStxPhysicalDisk::sector(
    std::uint8_t track, std::uint8_t side, std::uint8_t id) const {
    const auto found = std::find_if(sectors_.begin(), sectors_.end(),
        [track, side, id](const AtariStStxSector& sector) {
            return sector.track == track && sector.side == side && sector.id == id;
        });
    if (found == sectors_.end()) throw std::runtime_error("STX sector not present");
    return std::span(image_).subspan(found->payload_offset, found->payload_size);
}

} // namespace eon
