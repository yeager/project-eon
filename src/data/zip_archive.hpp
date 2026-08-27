#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace eon {

enum class AssetKind {
    amiga_adf,
    atari_st_disk,
    dos_floppy_image,
    dos_flat_executable,
    dos_mz_executable,
    dos_com_program,
    audio,
    game_resource,
    unknown,
};

struct ZipEntry {
    std::string name;
    std::uint16_t method = 0;
    std::uint32_t crc32 = 0;
    std::uint32_t compressed_size = 0;
    std::uint32_t uncompressed_size = 0;
    std::uint32_t local_offset = 0;
    bool directory = false;
};

class ZipArchive {
public:
    explicit ZipArchive(std::vector<std::uint8_t> bytes);
    static ZipArchive open(const std::filesystem::path& path);

    [[nodiscard]] const std::vector<ZipEntry>& entries() const { return entries_; }
    [[nodiscard]] std::vector<std::uint8_t> extract(const ZipEntry& entry) const;

private:
    std::vector<std::uint8_t> bytes_;
    std::vector<ZipEntry> entries_;
};

struct ArchiveAsset {
    std::string path;
    std::uint64_t size = 0;
    std::string sha256;
    AssetKind kind = AssetKind::unknown;
};

[[nodiscard]] std::string name(AssetKind kind);

[[nodiscard]] std::vector<ArchiveAsset> inventory_zip(
    const std::filesystem::path& path,
    unsigned maximum_nesting = 2);

} // namespace eon
