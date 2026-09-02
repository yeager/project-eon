#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
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
    std::uint16_t flags = 0;
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

    // Read a supplied outer archive once, verify those exact in-memory bytes,
    // then parse them.  This closes the scan-to-use gap: no caller can first
    // fingerprint one on-disk version and subsequently extract another.
    static ZipArchive open_verified(const std::filesystem::path& path,
                                    std::string_view expected_sha256);

    [[nodiscard]] const std::vector<ZipEntry>& entries() const { return entries_; }
    [[nodiscard]] std::vector<std::uint8_t> extract(const ZipEntry& entry) const;
    // Walk this already-admitted in-memory archive without reopening its
    // source path. Nested ZIP members are decoded transiently and remain
    // bounded by the same parser limits as path-based extraction.
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> extract_asset_by_sha256(
        std::string_view expected_sha256, unsigned maximum_nesting = 2) const;

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
// Classification is metadata-only and shared by archive and verified direct
// media inventories; it never changes or interprets source bytes.
[[nodiscard]] AssetKind classify_asset(std::string_view path,
                                       std::span<const std::uint8_t> bytes);

[[nodiscard]] std::vector<ArchiveAsset> inventory_zip(
    const std::filesystem::path& path,
    unsigned maximum_nesting = 2);
[[nodiscard]] std::optional<std::vector<std::uint8_t>> extract_asset_by_sha256(
    const std::filesystem::path& path,
    std::string_view expected_sha256,
    unsigned maximum_nesting = 2);

// As above, but validates the complete supplied outer archive before walking
// its nested members.  The outer identity is intentionally separate from the
// leaf identity: a matching resource in a different release is not admitted.
[[nodiscard]] std::vector<ArchiveAsset> inventory_verified_zip(
    const std::filesystem::path& path,
    std::string_view expected_archive_sha256,
    unsigned maximum_nesting = 2);
[[nodiscard]] std::optional<std::vector<std::uint8_t>> extract_verified_asset_by_sha256(
    const std::filesystem::path& path,
    std::string_view expected_archive_sha256,
    std::string_view expected_asset_sha256,
    unsigned maximum_nesting = 2);

} // namespace eon
