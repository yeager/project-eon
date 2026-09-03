#include "data/fat12.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace eon {
namespace {

std::uint16_t little16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) throw std::runtime_error("Truncated FAT12 field");
    return static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
}

std::uint32_t little32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) throw std::runtime_error("Truncated FAT12 field");
    return static_cast<std::uint32_t>(bytes[offset])
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U)
        | (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U)
        | (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
}

std::string trim_name(std::span<const std::uint8_t> field) {
    std::string result;
    for (const auto value : field) {
        if (value == ' ') break;
        result.push_back(static_cast<char>(value));
    }
    return result;
}

std::string lower(std::string_view text) {
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return result;
}

} // namespace

Fat12Disk::Fat12Disk(const std::span<const std::uint8_t> image) : image_(image) {
    parse();
}

Fat12Disk::Fat12Disk(std::vector<std::uint8_t> image)
    : owned_image_(std::move(image)), image_(owned_image_) {
    parse();
}

void Fat12Disk::parse() {
    if (image_.size() < 512) throw std::runtime_error("FAT12 image is too short");
    bytes_per_sector_ = little16(image_, 11);
    sectors_per_cluster_ = image_[13];
    reserved_sectors_ = little16(image_, 14);
    fat_count_ = image_[16];
    const auto root_entry_count = little16(image_, 17);
    auto total_sectors = static_cast<std::uint32_t>(little16(image_, 19));
    if (total_sectors == 0) total_sectors = little32(image_, 32);
    sectors_per_fat_ = little16(image_, 22);
    if ((bytes_per_sector_ != 512 && bytes_per_sector_ != 1024 && bytes_per_sector_ != 2048)
        || sectors_per_cluster_ == 0 || reserved_sectors_ == 0 || fat_count_ == 0
        || sectors_per_fat_ == 0 || total_sectors == 0
        || static_cast<std::uint64_t>(total_sectors) * bytes_per_sector_ > image_.size()) {
        throw std::runtime_error("Invalid FAT12 BIOS parameter block");
    }
    fat_offset_ = static_cast<std::size_t>(reserved_sectors_) * bytes_per_sector_;
    const auto root_offset = static_cast<std::size_t>(reserved_sectors_ + fat_count_ * sectors_per_fat_)
        * bytes_per_sector_;
    const auto root_bytes = static_cast<std::size_t>(root_entry_count) * 32U;
    const auto root_sectors = (root_bytes + bytes_per_sector_ - 1U) / bytes_per_sector_;
    data_offset_ = root_offset + root_sectors * bytes_per_sector_;
    if (root_offset > image_.size() || root_bytes > image_.size() - root_offset
        || data_offset_ > image_.size()) {
        throw std::runtime_error("FAT12 regions outside image");
    }
    for (std::size_t index = 0; index < root_entry_count; ++index) {
        const auto offset = root_offset + index * 32U;
        const auto first = image_[offset];
        const auto attributes = image_[offset + 11];
        if (first == 0) break;
        if (first == 0xe5 || attributes == 0x0f || (attributes & 0x08U) != 0) continue;
        auto filename = trim_name(image_.subspan(offset, 8));
        const auto extension = trim_name(image_.subspan(offset + 8, 3));
        if (!extension.empty()) filename += "." + extension;
        root_entries_.push_back({filename, attributes, little16(image_, offset + 26),
            little32(image_, offset + 28)});
    }
}

std::uint16_t Fat12Disk::next_cluster(std::uint16_t cluster) const {
    const auto offset = fat_offset_ + cluster + cluster / 2U;
    if (offset > image_.size() || image_.size() - offset < 2) throw std::runtime_error("FAT12 cluster outside FAT");
    auto value = little16(image_, offset);
    value = (cluster & 1U) == 0 ? static_cast<std::uint16_t>(value & 0x0fffU)
        : static_cast<std::uint16_t>(value >> 4U);
    return value;
}

std::vector<std::uint8_t> Fat12Disk::read(const Fat12Entry& entry) const {
    if (entry.directory()) throw std::runtime_error("Cannot read FAT12 directory as file");
    std::vector<std::uint8_t> result;
    result.reserve(entry.size);
    if (entry.size == 0) return result;
    auto cluster = entry.first_cluster;
    if (cluster < 2) throw std::runtime_error("Invalid FAT12 first cluster");
    const auto cluster_size = static_cast<std::size_t>(sectors_per_cluster_) * bytes_per_sector_;
    std::vector<bool> visited(image_.size() / cluster_size + 2U, false);
    while (result.size() < entry.size) {
        if (cluster < 2 || cluster >= 0xff8 || cluster >= visited.size() || visited[cluster]) {
            throw std::runtime_error("Invalid or cyclic FAT12 cluster chain");
        }
        visited[cluster] = true;
        const auto offset = data_offset_ + static_cast<std::size_t>(cluster - 2U) * cluster_size;
        if (offset > image_.size() || cluster_size > image_.size() - offset) {
            throw std::runtime_error("FAT12 data cluster outside image");
        }
        const auto remaining = static_cast<std::size_t>(entry.size) - result.size();
        const auto count = std::min(cluster_size, remaining);
        result.insert(result.end(), image_.begin() + static_cast<std::ptrdiff_t>(offset),
            image_.begin() + static_cast<std::ptrdiff_t>(offset + count));
        if (result.size() < entry.size) cluster = next_cluster(cluster);
    }
    return result;
}

const Fat12Entry* Fat12Disk::find(std::string_view name) const {
    const auto wanted = lower(name);
    const auto found = std::find_if(root_entries_.begin(), root_entries_.end(),
        [&wanted](const auto& entry) { return lower(entry.name) == wanted; });
    return found == root_entries_.end() ? nullptr : &*found;
}

} // namespace eon
