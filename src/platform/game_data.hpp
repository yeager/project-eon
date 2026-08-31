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
    // These two rejection counters make the admission boundary observable
    // without reporting user filenames. A size rejection was never hashed;
    // a hash rejection had a manifest-sized byte stream but no manifest
    // content identity. Neither is a supported release or a fallback.
    std::size_t size_rejected_candidates = 0;
    std::size_t size_candidates = 0;
    std::size_t hashed_candidates = 0;
    std::size_t hash_rejected_candidates = 0;
    std::size_t verified_occurrences = 0;
    std::size_t duplicate_occurrences = 0;
    std::size_t unreadable_candidates = 0;
};

// A bounded, read-only scan over a user-selected directory. Both directory
// discovery and hashing advance through the same explicit work budget, so the
// launcher can draw before a large Downloads directory has been enumerated.
// Candidate paths are sorted only after discovery completes; recognition
// remains content-addressed and duplicate selection stays deterministic.
class ReleaseScanner {
public:
    explicit ReleaseScanner(const std::filesystem::path& directory);

    // Visit or hash at most max_files filesystem entries/candidates. Returns
    // true once discovery and content hashing are both complete.
    bool advance(std::size_t max_files = 1);
    [[nodiscard]] bool done() const {
        return candidate_inventory_complete_ && next_candidate_ >= candidates_.size();
    }
    [[nodiscard]] bool discovering() const { return !candidate_inventory_complete_; }
    [[nodiscard]] std::size_t scanned_count() const { return next_candidate_; }
    // This is the number discovered so far while enumeration is active, and
    // the final candidate total once it completes. Callers must consult
    // discovering() before presenting it as a complete total.
    [[nodiscard]] std::size_t candidate_count() const { return candidates_.size(); }
    [[nodiscard]] const std::vector<ReleaseArchive>& releases() const { return releases_; }
    [[nodiscard]] const ReleaseScanReport& report() const { return report_; }

private:
    void finish_candidate_inventory();

    std::vector<std::filesystem::path> candidates_;
    std::vector<std::filesystem::path> directories_;
    std::size_t next_directory_ = 0;
    std::filesystem::directory_iterator active_directory_;
    bool candidate_inventory_complete_ = false;
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
