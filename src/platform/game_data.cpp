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
    std::error_code error;
    if (std::filesystem::is_regular_file(directory, error) && !error) {
        candidates_.push_back(directory);
        finish_candidate_inventory();
        return;
    }
    error.clear();
    if (std::filesystem::is_directory(directory, error) && !error) {
        directories_.push_back(directory);
    } else {
        // A missing default data directory is an empty, completed scan. The
        // lookup itself never creates it.
        finish_candidate_inventory();
    }
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
        if (entry.is_directory(type_error) && !type_error && !entry.is_symlink(type_error)) {
            directories_.push_back(entry.path());
            continue;
        }
        type_error.clear();
        if (entry.is_regular_file(type_error) && !type_error) candidates_.push_back(entry.path());
    }

    while (remaining != 0 && candidate_inventory_complete_ && next_candidate_ < candidates_.size()) {
        const auto& candidate = candidates_[next_candidate_++];
        --remaining;
        try {
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
