#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace eon {

// A read-only index over an STX physical floppy dump.  It deliberately does
// not produce a flat .st image: callers retain the supplied physical bytes and
// request identified sectors from their recorded locations.
struct AtariStStxSector {
    std::uint8_t track = 0;
    std::uint8_t side = 0;
    std::uint8_t id = 0;
    std::uint8_t size_code = 0;
    std::uint8_t fdc_status = 0;
    std::size_t payload_offset = 0;
    std::size_t payload_size = 0;
};

class AtariStStxPhysicalDisk {
public:
    explicit AtariStStxPhysicalDisk(std::vector<std::uint8_t> image);

    [[nodiscard]] std::size_t track_count() const { return track_count_; }
    [[nodiscard]] const std::vector<AtariStStxSector>& sectors() const { return sectors_; }
    [[nodiscard]] std::span<const std::uint8_t> sector(
        std::uint8_t track, std::uint8_t side, std::uint8_t id) const;

private:
    std::vector<std::uint8_t> image_;
    std::size_t track_count_ = 0;
    std::vector<AtariStStxSector> sectors_;
};

// A deliberately narrow FAT12 view over identified physical STX sectors. It
// never constructs a flat .st image or returns file contents: its purpose is
// to establish whether the on-media BPB, mirrored FATs and root records form
// a credible sector-backed namespace for later, separately proven readers.
struct AtariStStxFat12RootEntry {
    std::string name;
    std::uint8_t attributes = 0;
    std::uint16_t first_cluster = 0;
    std::uint32_t size = 0;
};

class AtariStStxFat12Root {
public:
    explicit AtariStStxFat12Root(const AtariStStxPhysicalDisk& disk);

    [[nodiscard]] std::uint16_t bytes_per_sector() const { return bytes_per_sector_; }
    [[nodiscard]] std::uint8_t sectors_per_cluster() const { return sectors_per_cluster_; }
    [[nodiscard]] std::uint16_t total_sectors() const { return total_sectors_; }
    [[nodiscard]] std::uint16_t fat_start_lba() const { return reserved_sectors_; }
    [[nodiscard]] std::uint16_t sectors_per_fat() const { return sectors_per_fat_; }
    [[nodiscard]] bool fat_mirrors_match() const { return fat_mirrors_match_; }
    [[nodiscard]] const std::string& fat_primary_sha256() const { return fat_primary_sha256_; }
    [[nodiscard]] const std::string& fat_secondary_sha256() const { return fat_secondary_sha256_; }
    [[nodiscard]] std::uint16_t root_start_lba() const { return root_start_lba_; }
    [[nodiscard]] std::uint16_t root_sector_count() const { return root_sector_count_; }
    [[nodiscard]] const std::vector<AtariStStxFat12RootEntry>& entries() const { return entries_; }

private:
    std::span<const std::uint8_t> logical_sector(std::uint16_t lba) const;
    std::uint16_t next_cluster(std::uint16_t cluster) const;
    void validate_mirrored_fats();
    void validate_file_chain(const AtariStStxFat12RootEntry& entry) const;

    const AtariStStxPhysicalDisk& disk_;
    std::uint16_t bytes_per_sector_ = 0;
    std::uint8_t sectors_per_cluster_ = 0;
    std::uint16_t reserved_sectors_ = 0;
    std::uint8_t fat_count_ = 0;
    std::uint16_t sectors_per_fat_ = 0;
    std::uint16_t sectors_per_track_ = 0;
    std::uint16_t head_count_ = 0;
    std::uint16_t total_sectors_ = 0;
    std::uint16_t root_start_lba_ = 0;
    std::uint16_t root_sector_count_ = 0;
    std::uint16_t data_start_lba_ = 0;
    bool fat_mirrors_match_ = false;
    std::string fat_primary_sha256_;
    std::string fat_secondary_sha256_;
    std::vector<AtariStStxFat12RootEntry> entries_;
};

} // namespace eon
