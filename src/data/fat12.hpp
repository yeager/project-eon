#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace eon {

struct Fat12Entry {
    std::string name;
    std::uint8_t attributes = 0;
    std::uint16_t first_cluster = 0;
    std::uint32_t size = 0;

    [[nodiscard]] bool directory() const { return (attributes & 0x10U) != 0; }
};

class Fat12Disk {
public:
    // Inspection callers can borrow a verified disk image. The source must
    // outlive this reader and all of its direct raw-media views.
    explicit Fat12Disk(std::span<const std::uint8_t> image);
    // Long-lived sessions may take the already read disk image exactly once.
    explicit Fat12Disk(std::vector<std::uint8_t> image);

    [[nodiscard]] std::uint16_t bytes_per_sector() const { return bytes_per_sector_; }
    [[nodiscard]] std::uint8_t sectors_per_cluster() const { return sectors_per_cluster_; }
    // Read-only raw provenance for media-owning sessions. Callers must not
    // retain or mutate the underlying image through this view.
    [[nodiscard]] std::span<const std::uint8_t> bytes() const { return image_; }
    [[nodiscard]] const std::vector<Fat12Entry>& root_entries() const { return root_entries_; }
    [[nodiscard]] std::vector<std::uint8_t> read(const Fat12Entry& entry) const;
    [[nodiscard]] const Fat12Entry* find(std::string_view name) const;

private:
    void parse();
    [[nodiscard]] std::uint16_t next_cluster(std::uint16_t cluster) const;

    std::vector<std::uint8_t> owned_image_;
    std::span<const std::uint8_t> image_;
    std::uint16_t bytes_per_sector_ = 0;
    std::uint8_t sectors_per_cluster_ = 0;
    std::uint16_t reserved_sectors_ = 0;
    std::uint8_t fat_count_ = 0;
    std::uint16_t sectors_per_fat_ = 0;
    std::size_t fat_offset_ = 0;
    std::size_t data_offset_ = 0;
    std::vector<Fat12Entry> root_entries_;
};

} // namespace eon
