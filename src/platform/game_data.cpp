#include "platform/game_data.hpp"

#include "data/release_manifest.hpp"
#include "data/sha256.hpp"
#include "data/zip_archive.hpp"

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <system_error>

namespace eon {
namespace {

const ReleaseManifestEntry& require_manifest_identity(const ReleaseArchive& release) {
    const auto manifest = release_manifest();
    const auto found = std::find_if(manifest.begin(), manifest.end(), [&release](const auto& candidate) {
        return candidate.sha256 == release.sha256
            && candidate.game == release.game
            && candidate.platform == release.platform
            && candidate.language == release.language;
    });
    if (found == manifest.end()) {
        throw std::runtime_error("Release metadata is not an exact recognised manifest identity");
    }
    return *found;
}

} // namespace

OriginalDataSourceKind classify_original_data_source(const std::filesystem::path& path) {
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error || status.type() == std::filesystem::file_type::not_found) {
        return OriginalDataSourceKind::missing;
    }
    if (std::filesystem::is_symlink(status)) return OriginalDataSourceKind::unsupported;
    if (std::filesystem::is_directory(status)) return OriginalDataSourceKind::directory;
    if (std::filesystem::is_regular_file(status)) return OriginalDataSourceKind::archive;
    return OriginalDataSourceKind::unsupported;
}

bool is_original_data_source(const OriginalDataSourceKind kind) {
    return kind == OriginalDataSourceKind::directory || kind == OriginalDataSourceKind::archive;
}

std::string_view name(const OriginalDataSourceKind kind) {
    switch (kind) {
    case OriginalDataSourceKind::missing: return "missing";
    case OriginalDataSourceKind::directory: return "directory";
    case OriginalDataSourceKind::archive: return "archive";
    case OriginalDataSourceKind::unsupported: return "unsupported";
    }
    return "unsupported";
}

std::vector<ReleaseArchive> find_release_archives(const std::filesystem::path& directory) {
    ReleaseScanner scanner(directory);
    while (!scanner.advance(64)) {
    }
    return scanner.releases();
}

void verify_release_archive(const ReleaseArchive& release) {
    static_cast<void>(require_manifest_identity(release));
    static_cast<void>(ZipArchive::open_verified(release.path, release.sha256));
}

std::vector<ArchiveAsset> inventory_verified_release(const ReleaseArchive& release) {
    static_cast<void>(require_manifest_identity(release));
    return inventory_verified_zip(release.path, release.sha256);
}

std::optional<std::vector<std::uint8_t>> extract_verified_release_asset(
    const ReleaseArchive& release, std::string_view expected_asset_sha256) {
    static_cast<void>(require_manifest_identity(release));
    return extract_verified_asset_by_sha256(release.path, release.sha256, expected_asset_sha256);
}

ReleaseScanner::ReleaseScanner(const std::filesystem::path& directory) {
    report_.source_kind = classify_original_data_source(directory);
    switch (report_.source_kind) {
    case OriginalDataSourceKind::archive:
        candidates_.push_back(directory);
        finish_candidate_inventory();
        return;
    case OriginalDataSourceKind::directory:
        directories_.push_back(directory);
        return;
    case OriginalDataSourceKind::missing:
        // A missing default data directory is an empty, completed scan. The
        // lookup itself never creates it.
        finish_candidate_inventory();
        return;
    case OriginalDataSourceKind::unsupported:
        // Do not traverse devices, FIFOs, sockets, or symlinks. They cannot
        // provide an exact user-selected original-media identity.
        finish_candidate_inventory();
        return;
    }
}

ReleaseScanSnapshot ReleaseScanner::snapshot() const {
    return {report_.source_kind, discovering(), done(), candidate_count(), scanned_count(),
        releases_.size(), report_};
}

void ReleaseScanner::finish_candidate_inventory() {
    std::sort(candidates_.begin(), candidates_.end());
    report_.candidates = candidates_.size();
    candidate_inventory_complete_ = true;
}

bool ReleaseScanner::advance(std::size_t max_files) {
    std::size_t remaining = max_files;
    while (remaining != 0 && !candidate_inventory_complete_) {
        if (active_directory_ == std::filesystem::directory_iterator{}) {
            if (next_directory_ == directories_.size()) {
                finish_candidate_inventory();
                break;
            }
            std::error_code error;
            active_directory_ = std::filesystem::directory_iterator(
                directories_[next_directory_++],
                std::filesystem::directory_options::skip_permission_denied, error);
            if (error) {
                // Directory traversal has no candidate to account for. This
                // matches the previous recursive scanner: unreadable files,
                // not inaccessible directories, are part of the aggregate
                // candidate accounting contract.
                active_directory_ = {};
                continue;
            }
            if (active_directory_ == std::filesystem::directory_iterator{}) continue;
        }

        const auto entry = *active_directory_;
        std::error_code increment_error;
        active_directory_.increment(increment_error);
        if (increment_error) {
            active_directory_ = {};
        }
        --remaining;

        std::error_code type_error;
        const auto type = entry.symlink_status(type_error);
        if (type_error) continue;
        if (std::filesystem::is_symlink(type)) {
            ++report_.symlink_rejected_entries;
            continue;
        }
        if (std::filesystem::is_directory(type)) {
            directories_.push_back(entry.path());
            continue;
        }
        if (std::filesystem::is_regular_file(type)) candidates_.push_back(entry.path());
    }

    while (remaining != 0 && candidate_inventory_complete_ && next_candidate_ < candidates_.size()) {
        const auto& candidate = candidates_[next_candidate_++];
        --remaining;
        try {
            if (classify_original_data_source(candidate) != OriginalDataSourceKind::archive) {
                ++report_.unreadable_candidates;
                continue;
            }
            const auto size = std::filesystem::file_size(candidate);
            const auto manifest = release_manifest();
            if (std::none_of(manifest.begin(), manifest.end(),
                    [size](const auto& known) { return known.size == size; })) {
                ++report_.size_rejected_candidates;
                continue;
            }
            ++report_.size_candidates;
            const auto fingerprint = to_hex(sha256_file(candidate));
            ++report_.hashed_candidates;
            const auto found = std::find_if(manifest.begin(), manifest.end(),
                [&fingerprint](const auto& known) { return fingerprint == known.sha256; });
            if (found != manifest.end()) {
                ++report_.verified_occurrences;
                const auto existing = std::find_if(releases_.begin(), releases_.end(),
                    [&fingerprint](const auto& release) { return release.sha256 == fingerprint; });
                if (existing == releases_.end()) {
                    releases_.push_back({found->game, found->platform, std::string(found->language),
                        fingerprint, candidate});
                } else {
                    // Candidates are lexically sorted before scanning. Keep
                    // the first path as a deterministic in-place read target
                    // and record every additional copy/link as evidence only.
                    ++report_.duplicate_occurrences;
                }
            } else {
                // The complete outer archive was read but does not have a
                // recognised identity. Keep this aggregate evidence so an
                // alternate/cracked/repacked release cannot disappear into
                // the generic candidate total.
                ++report_.hash_rejected_candidates;
            }
        } catch (const std::exception&) {
            // A file may disappear during a scan. It is simply not a verified
            // release; no filename fallback is allowed.
            ++report_.unreadable_candidates;
        }
    }
    if (!done()) return false;
    std::sort(releases_.begin(), releases_.end(), [](const auto& left, const auto& right) {
        return left.sha256 < right.sha256;
    });
    return true;
}

std::string name(Game game) {
    return game == Game::millennium ? "Millennium 2.2" : "Deuteros";
}

std::string name(Platform platform) {
    switch (platform) {
    case Platform::dos: return "DOS";
    case Platform::amiga: return "Amiga";
    case Platform::atari_st: return "Atari ST";
    }
    return "Unknown";
}

} // namespace eon
