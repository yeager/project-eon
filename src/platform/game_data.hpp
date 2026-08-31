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

// A hash-recognised physical leaf encountered outside a recognised release
// container.  It is deliberately *not* a ReleaseArchive: one disk can be
// shared by several container releases, and a leaf alone does not prove that
// every disk/file required by an original release is present.  Keeping this
// evidence separate lets the scanner preserve direct ADF/ST/DOS-image facts
// without inventing a launchable release identity.
struct UnboundDirectMedia {
    std::string sha256;
    std::uint64_t size = 0;
    std::filesystem::path path;
};

// Every caller must classify an original-media root through this one boundary.
// A directory is enumerated incrementally; a regular file is one candidate
// archive.  Symlinks and every other filesystem object are deliberately not
// accepted: selecting one must not silently redirect a hash-bound scan to a
// different collection after the source was chosen.
enum class OriginalDataSourceKind {
    missing,
    directory,
    archive,
    unsupported,
};

[[nodiscard]] OriginalDataSourceKind classify_original_data_source(
    const std::filesystem::path& path);
[[nodiscard]] bool is_original_data_source(OriginalDataSourceKind kind);
[[nodiscard]] std::string_view name(OriginalDataSourceKind kind);

// Counts are deliberately aggregate-only: a preservation scan must make its
// admission decision auditable without exposing unrecognised filenames or
// treating them as a fallback catalogue.  A "verified occurrence" is one
// read of a complete outer archive whose hash matches the manifest.  Multiple
// occurrences of the same content identity are represented by one release.
struct ReleaseScanReport {
    OriginalDataSourceKind source_kind = OriginalDataSourceKind::missing;
    std::size_t candidates = 0;
    // These two rejection counters make the admission boundary observable
    // without reporting user filenames. A size rejection was never hashed;
    // a hash rejection had a manifest-sized byte stream but no manifest
    // content identity. Neither is a supported release or a fallback.
    std::size_t size_rejected_candidates = 0;
    std::size_t size_candidates = 0;
    std::size_t hashed_candidates = 0;
    std::size_t hash_rejected_candidates = 0;
    // Direct physical media are independently hash-addressed against leaf
    // evidence from the parser manifest. They never increase the release
    // count and cannot make a launcher card startable until a complete direct
    // media-set identity is documented in the manifest.
    std::size_t direct_media_size_candidates = 0;
    std::size_t direct_media_hashed_candidates = 0;
    std::size_t verified_direct_media_occurrences = 0;
    std::size_t duplicate_direct_media_occurrences = 0;
    std::size_t verified_occurrences = 0;
    std::size_t duplicate_occurrences = 0;
    std::size_t unreadable_candidates = 0;
    // Links are rejected before either traversal or candidate hashing. This
    // keeps a selected collection from silently reaching media outside its
    // explicit directory boundary while preserving only an aggregate count.
    std::size_t symlink_rejected_entries = 0;
};

// An aggregate-only view of one scanner instant. It intentionally contains no
// candidate path, filename, archive member, or media byte. The card menu and
// CLI diagnostics consume this same value so neither presentation can invent
// a separate interpretation of missing or rejected original data.
struct ReleaseScanSnapshot {
    OriginalDataSourceKind source_kind = OriginalDataSourceKind::missing;
    bool discovering = false;
    bool complete = false;
    std::size_t candidate_count = 0;
    std::size_t scanned_count = 0;
    std::size_t unique_release_count = 0;
    std::size_t unique_unbound_direct_media_count = 0;
    ReleaseScanReport report;
};

// A bounded, read-only scan over a user-selected directory or one archive.
// Both directory discovery and hashing advance through the same explicit work
// budget, so the launcher can draw before a large Downloads directory has
// been enumerated. Candidate paths are sorted only after discovery completes;
// recognition remains content-addressed and duplicate selection stays
// deterministic.
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
    [[nodiscard]] const std::vector<UnboundDirectMedia>& unbound_direct_media() const {
        return unbound_direct_media_;
    }
    [[nodiscard]] const ReleaseScanReport& report() const { return report_; }
    [[nodiscard]] ReleaseScanSnapshot snapshot() const;

private:
    void finish_candidate_inventory();

    std::vector<std::filesystem::path> candidates_;
    std::vector<std::filesystem::path> directories_;
    std::size_t next_directory_ = 0;
    std::filesystem::directory_iterator active_directory_;
    bool candidate_inventory_complete_ = false;
    std::size_t next_candidate_ = 0;
    std::vector<ReleaseArchive> releases_;
    std::vector<UnboundDirectMedia> unbound_direct_media_;
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
