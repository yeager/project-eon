#pragma once

#include "data/zip_archive.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace eon {

enum class Game { millennium, deuteros };
enum class Platform { dos, amiga, atari_st };

struct ReleaseArchive {
    Game game;
    Platform platform;
    std::string language;
    std::string sha256;
    std::filesystem::path path;
};

// Counts are deliberately aggregate-only: a preservation scan must make its
// admission decision auditable without exposing unrecognised filenames or
// treating them as a fallback catalogue.  A "verified occurrence" is one
// read of a complete outer archive whose hash matches the manifest.  Multiple
// occurrences of the same content identity are represented by one release.
struct ReleaseScanReport {
    std::size_t candidates = 0;
    std::size_t size_candidates = 0;
    std::size_t hashed_candidates = 0;
    std::size_t verified_occurrences = 0;
    std::size_t duplicate_occurrences = 0;
    std::size_t unreadable_candidates = 0;
};

// A bounded, read-only scan over a user-selected directory.  The launcher
// advances this while rendering so data verification never replaces its first
// frame with a blocking hash pass.  Recognition remains content-addressed.
class ReleaseScanner {
public:
    explicit ReleaseScanner(const std::filesystem::path& directory);

    // Hash at most max_files candidates. Returns true once the scan is done.
    bool advance(std::size_t max_files = 1);
    [[nodiscard]] bool done() const { return next_candidate_ >= candidates_.size(); }
    [[nodiscard]] std::size_t scanned_count() const { return next_candidate_; }
    [[nodiscard]] std::size_t candidate_count() const { return candidates_.size(); }
    [[nodiscard]] const std::vector<ReleaseArchive>& releases() const { return releases_; }
    [[nodiscard]] const ReleaseScanReport& report() const { return report_; }

private:
    std::vector<std::filesystem::path> candidates_;
    std::size_t next_candidate_ = 0;
    std::vector<ReleaseArchive> releases_;
    ReleaseScanReport report_;
};

[[nodiscard]] std::vector<ReleaseArchive> find_release_archives(
    const std::filesystem::path& directory);
// Re-open an already discovered release only through its full manifest
// identity. These helpers verify the exact in-memory outer bytes used for the
// following archive walk, so recognised media cannot be swapped after scan.
void verify_release_archive(const ReleaseArchive& release);
[[nodiscard]] std::vector<ArchiveAsset> inventory_verified_release(const ReleaseArchive& release);
[[nodiscard]] std::optional<std::vector<std::uint8_t>> extract_verified_release_asset(
    const ReleaseArchive& release, std::string_view expected_asset_sha256);
[[nodiscard]] std::string name(Game game);
[[nodiscard]] std::string name(Platform platform);

} // namespace eon
