#include "data/atari_st_stx.hpp"

#include "data/sha256.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
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

std::string trim_name(std::span<const std::uint8_t> field) {
    std::string result;
    for (const auto byte : field) {
        if (byte == ' ') break;
        if (byte < 0x20 || byte > 0x7e) throw std::runtime_error("Invalid STX FAT12 root filename");
        result.push_back(static_cast<char>(byte));
    }
    return result;
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

AtariStStxFat12Root::AtariStStxFat12Root(const AtariStStxPhysicalDisk& disk)
    : disk_(disk) {
    const auto boot = disk_.sector(0, 0, 1);
    if (boot.size() != 512) throw std::runtime_error("STX FAT12 boot sector is not 512 bytes");
    bytes_per_sector_ = little16(boot, 11);
    sectors_per_cluster_ = boot[13];
    reserved_sectors_ = little16(boot, 14);
    fat_count_ = boot[16];
    const auto root_entry_count = little16(boot, 17);
    total_sectors_ = little16(boot, 19);
    sectors_per_fat_ = little16(boot, 22);
    sectors_per_track_ = little16(boot, 24);
    head_count_ = little16(boot, 26);
    if (bytes_per_sector_ != 512 || sectors_per_cluster_ == 0 || reserved_sectors_ == 0
        || fat_count_ != 2 || root_entry_count == 0 || total_sectors_ == 0
        || sectors_per_fat_ == 0 || sectors_per_track_ == 0 || head_count_ == 0) {
        throw std::runtime_error("Invalid STX FAT12 BIOS parameter block");
    }
    const auto root_bytes = static_cast<std::uint32_t>(root_entry_count) * 32U;
    root_sector_count_ = static_cast<std::uint16_t>((root_bytes + bytes_per_sector_ - 1U) / bytes_per_sector_);
    const auto root_start = static_cast<std::uint32_t>(reserved_sectors_)
        + static_cast<std::uint32_t>(fat_count_) * sectors_per_fat_;
    const auto data_start = root_start + root_sector_count_;
    if (root_start > total_sectors_ || data_start > total_sectors_) {
        throw std::runtime_error("STX FAT12 regions exceed declared medium");
    }
    root_start_lba_ = static_cast<std::uint16_t>(root_start);
    data_start_lba_ = static_cast<std::uint16_t>(data_start);
    validate_mirrored_fats();
    for (std::uint16_t index = 0; index < root_entry_count; ++index) {
        const auto sector = logical_sector(static_cast<std::uint16_t>(root_start_lba_ + index / 16U));
        const auto offset = static_cast<std::size_t>(index % 16U) * 32U;
        const auto first = sector[offset];
        const auto attributes = sector[offset + 11U];
        if (first == 0) break;
        if (first == 0xe5 || attributes == 0x0f || (attributes & 0x08U) != 0) continue;
        auto name = trim_name(sector.subspan(offset, 8));
        const auto extension = trim_name(sector.subspan(offset + 8U, 3));
        if (name.empty()) throw std::runtime_error("Empty STX FAT12 root filename");
        if (!extension.empty()) name += "." + extension;
        entries_.push_back({std::move(name), attributes, little16(sector, offset + 26U),
            little32(sector, offset + 28U)});
    }
    if (entries_.empty()) throw std::runtime_error("STX FAT12 root has no live entries");
    // A divergent physical mirror is itself preservation evidence. Do not
    // silently choose the first copy to certify a file chain; retain the root
    // records, but only validate chains when the two originals agree.
    if (fat_mirrors_match_) {
        for (const auto& entry : entries_) validate_file_chain(entry);
    }
}

void AtariStStxFat12Root::validate_mirrored_fats() {
    // FAT copies are physical-sector metadata, not a synthesized flat disk.
    // Hash both addressed copies before following a cluster link. A mismatch
    // remains observable and prevents later chain certification rather than
    // silently choosing one physical copy.
    if (fat_count_ != 2 || sectors_per_fat_ == 0) {
        throw std::runtime_error("Invalid STX FAT12 mirror layout");
    }
    std::vector<std::uint8_t> primary;
    std::vector<std::uint8_t> secondary;
    primary.reserve(static_cast<std::size_t>(sectors_per_fat_) * bytes_per_sector_);
    secondary.reserve(static_cast<std::size_t>(sectors_per_fat_) * bytes_per_sector_);
    for (std::uint16_t offset = 0; offset < sectors_per_fat_; ++offset) {
        const auto first_lba = static_cast<std::uint32_t>(reserved_sectors_) + offset;
        const auto second_lba = first_lba + sectors_per_fat_;
        if (second_lba >= total_sectors_) {
            throw std::runtime_error("STX FAT12 mirror sector outside declared medium");
        }
        const auto first = logical_sector(static_cast<std::uint16_t>(first_lba));
        const auto second = logical_sector(static_cast<std::uint16_t>(second_lba));
        if (first.size() != bytes_per_sector_ || second.size() != bytes_per_sector_) {
            throw std::runtime_error("STX FAT12 mirror sector has unexpected size");
        }
        primary.insert(primary.end(), first.begin(), first.end());
        secondary.insert(secondary.end(), second.begin(), second.end());
    }
    fat_primary_sha256_ = to_hex(sha256(primary));
    fat_secondary_sha256_ = to_hex(sha256(secondary));
    fat_mirrors_match_ = primary == secondary;
}

std::span<const std::uint8_t> AtariStStxFat12Root::logical_sector(const std::uint16_t lba) const {
    if (lba >= total_sectors_) throw std::runtime_error("STX FAT12 logical sector outside declared medium");
    const auto sectors_per_cylinder = static_cast<std::uint32_t>(sectors_per_track_) * head_count_;
    if (sectors_per_cylinder == 0) throw std::runtime_error("Invalid STX FAT12 geometry");
    const auto track = static_cast<std::uint32_t>(lba) / sectors_per_cylinder;
    const auto within_cylinder = static_cast<std::uint32_t>(lba) % sectors_per_cylinder;
    const auto side = within_cylinder / sectors_per_track_;
    const auto id = within_cylinder % sectors_per_track_ + 1U;
    if (track > std::numeric_limits<std::uint8_t>::max() || side > std::numeric_limits<std::uint8_t>::max()
        || id > std::numeric_limits<std::uint8_t>::max()) {
        throw std::runtime_error("STX FAT12 geometry exceeds sector identity range");
    }
    const auto result = disk_.sector(static_cast<std::uint8_t>(track), static_cast<std::uint8_t>(side),
        static_cast<std::uint8_t>(id));
    if (result.size() != bytes_per_sector_) throw std::runtime_error("STX FAT12 sector size differs from BPB");
    return result;
}

std::uint16_t AtariStStxFat12Root::next_cluster(const std::uint16_t cluster) const {
    const auto offset = static_cast<std::uint32_t>(cluster) + cluster / 2U;
    const auto fat_lba = static_cast<std::uint32_t>(reserved_sectors_) + offset / bytes_per_sector_;
    const auto within_sector = static_cast<std::size_t>(offset % bytes_per_sector_);
    if (fat_lba >= static_cast<std::uint32_t>(reserved_sectors_) + sectors_per_fat_
        || within_sector + 2U > bytes_per_sector_) {
        throw std::runtime_error("STX FAT12 cluster lies outside FAT");
    }
    const auto value = little16(logical_sector(static_cast<std::uint16_t>(fat_lba)), within_sector);
    return (cluster & 1U) == 0 ? static_cast<std::uint16_t>(value & 0x0fffU)
        : static_cast<std::uint16_t>(value >> 4U);
}

void AtariStStxFat12Root::validate_file_chain(const AtariStStxFat12RootEntry& entry) const {
    if ((entry.attributes & 0x10U) != 0) {
        throw std::runtime_error("STX FAT12 root directory entry is unsupported");
    }
    if (entry.size == 0) {
        if (entry.first_cluster != 0) throw std::runtime_error("STX FAT12 empty file has a cluster");
        return;
    }
    const auto cluster_bytes = static_cast<std::uint32_t>(sectors_per_cluster_) * bytes_per_sector_;
    const auto cluster_count = (entry.size + cluster_bytes - 1U) / cluster_bytes;
    const auto max_clusters = static_cast<std::uint32_t>(
        (total_sectors_ - data_start_lba_) / sectors_per_cluster_);
    if (entry.first_cluster < 2 || cluster_count > max_clusters) {
        throw std::runtime_error("Invalid STX FAT12 file cluster chain");
    }
    std::vector<bool> visited(static_cast<std::size_t>(max_clusters) + 2U, false);
    auto cluster = entry.first_cluster;
    for (std::uint32_t index = 0; index < cluster_count; ++index) {
        if (cluster < 2 || cluster >= visited.size() || visited[cluster]) {
            throw std::runtime_error("Invalid or cyclic STX FAT12 file cluster chain");
        }
        visited[cluster] = true;
        const auto first_sector = static_cast<std::uint32_t>(data_start_lba_)
            + static_cast<std::uint32_t>(cluster - 2U) * sectors_per_cluster_;
        if (first_sector >= total_sectors_ || sectors_per_cluster_ > total_sectors_ - first_sector) {
            throw std::runtime_error("STX FAT12 file cluster is outside declared medium");
        }
        if (index + 1U < cluster_count) cluster = next_cluster(cluster);
    }
}

} // namespace eon
