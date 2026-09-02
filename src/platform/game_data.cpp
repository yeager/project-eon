#include "platform/game_data.hpp"

#include "data/release_manifest.hpp"
#include "data/sha256.hpp"
#include "data/zip_archive.hpp"

#include <algorithm>
#include <exception>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <map>
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

const DirectMediaSetManifestEntry& require_direct_set_identity(const ReleaseArchive& release) {
    const auto sets = direct_media_set_manifest();
    const auto found = std::find_if(sets.begin(), sets.end(), [&release](const auto& candidate) {
        return candidate.content_release_sha256 == release.sha256
            && candidate.game == release.game && candidate.platform == release.platform
            && candidate.language == release.language;
    });
    if (found == sets.end()) {
        throw std::runtime_error("Release has no recognised direct-media set identity");
    }
    return *found;
}

std::vector<std::uint8_t> read_exact_regular_file(const std::filesystem::path& path,
                                                   const std::uint64_t expected_size) {
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error || std::filesystem::is_symlink(status) || !std::filesystem::is_regular_file(status)
        || std::filesystem::file_size(path, error) != expected_size || error) {
        throw std::runtime_error("Direct media member is not the declared regular file");
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Unable to open direct media member");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(expected_size));
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (stream.gcount() != static_cast<std::streamsize>(bytes.size()) || stream.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error("Direct media member changed while being read");
    }
    return bytes;
}

struct VerifiedDirectSet {
    std::vector<ArchiveAsset> inventory;
    std::map<std::string, std::vector<std::uint8_t>> assets;
};

VerifiedDirectSet verify_direct_set(const std::filesystem::path& root,
                                    const DirectMediaSetManifestEntry& set,
                                    const bool retain_bytes) {
    std::error_code error;
    const auto root_status = std::filesystem::symlink_status(root, error);
    if (error || std::filesystem::is_symlink(root_status) || !std::filesystem::is_directory(root_status)) {
        throw std::runtime_error("Direct media root is not a regular directory");
    }
    VerifiedDirectSet verified;
    std::string canonical;
    for (const auto& member : set.members) {
        // The declarative manifest permits bare, direct-child DOS names only.
        const std::filesystem::path name(member.name);
        if (name.empty() || name.has_parent_path() || name.filename() != name) {
            throw std::runtime_error("Unsafe direct-media manifest member name");
        }
        const auto path = root / name;
        auto bytes = read_exact_regular_file(path, member.size);
        const auto digest = to_hex(sha256(bytes));
        if (digest != member.sha256) throw std::runtime_error("Direct media member hash mismatch");
        canonical += std::string(member.name) + "\t" + std::to_string(member.size) + "\t" + digest + "\n";
        verified.inventory.push_back({std::string(member.name), member.size, digest,
            classify_asset(member.name, bytes)});
        if (retain_bytes && !verified.assets.emplace(digest, std::move(bytes)).second) {
            throw std::runtime_error("Ambiguous direct media asset hash");
        }
    }
    const auto canonical_bytes = std::span(reinterpret_cast<const std::uint8_t*>(canonical.data()), canonical.size());
    if (to_hex(sha256(canonical_bytes)) != set.set_sha256) {
        throw std::runtime_error("Direct media set identity mismatch");
    }
    return verified;
}

bool is_bound_direct_path(const std::vector<std::filesystem::path>& paths,
                          const std::filesystem::path& candidate) {
    return std::find(paths.begin(), paths.end(), candidate) != paths.end();
}

bool has_manifest_leaf_size(const std::uintmax_t size) {
    return std::any_of(parser_profile_manifest().begin(), parser_profile_manifest().end(),
        [size](const auto& profile) { return profile.leaf_size == size; });
}

bool is_manifest_leaf(const std::string_view sha256, const std::uintmax_t size) {
    return std::any_of(parser_profile_manifest().begin(), parser_profile_manifest().end(),
        [sha256, size](const auto& profile) {
            return profile.leaf_size == size && profile.leaf_sha256 == sha256;
        });
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

bool is_recognised_release_identity(const ReleaseArchive& release) {
    try {
        static_cast<void>(require_manifest_identity(release));
        if (release.layout == ReleaseMediaLayout::verified_directory) {
            static_cast<void>(require_direct_set_identity(release));
        }
        return true;
    } catch (...) {
        return false;
    }
}

std::optional<std::string> direct_media_set_sha256(const ReleaseArchive& release) {
    if (release.layout != ReleaseMediaLayout::verified_directory) return std::nullopt;
    try {
        return std::string(require_direct_set_identity(release).set_sha256);
    } catch (...) {
        return std::nullopt;
    }
}

void verify_release_archive(const ReleaseArchive& release) {
    static_cast<void>(VerifiedReleaseMedia::open(release));
}

VerifiedReleaseMedia VerifiedReleaseMedia::open(const ReleaseArchive& release) {
    static_cast<void>(require_manifest_identity(release));
    if (release.layout == ReleaseMediaLayout::zip_archive) {
        return VerifiedReleaseMedia(release, ZipArchive::open_verified(release.path, release.sha256));
    }
    const auto& set = require_direct_set_identity(release);
    auto verified = verify_direct_set(release.path, set, true);
    return VerifiedReleaseMedia(release, std::move(verified.inventory), std::move(verified.assets));
}

std::optional<std::vector<std::uint8_t>> VerifiedReleaseMedia::extract(
    const std::string_view expected_asset_sha256) const {
    if (archive_) return archive_->extract_asset_by_sha256(expected_asset_sha256);
    const auto found = direct_assets_.find(std::string(expected_asset_sha256));
    if (found == direct_assets_.end()) return std::nullopt;
    return found->second;
}

std::vector<ArchiveAsset> VerifiedReleaseMedia::inventory() const {
    if (!archive_) return direct_inventory_;
    // Inventory helpers intentionally reopen ZIP sources, so direct callers
    // use the already admitted snapshot. ZIP callers retain their established
    // helper semantics below.
    return inventory_verified_zip(release_.path, release_.sha256);
}

std::vector<ArchiveAsset> inventory_verified_release(const ReleaseArchive& release) {
    return VerifiedReleaseMedia::open(release).inventory();
}

std::optional<std::vector<std::uint8_t>> extract_verified_release_asset(
    const ReleaseArchive& release, std::string_view expected_asset_sha256) {
    return VerifiedReleaseMedia::open(release).extract(expected_asset_sha256);
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
        releases_.size(), unbound_direct_media_.size(), report_};
}

void ReleaseScanner::finish_candidate_inventory() {
    std::sort(candidates_.begin(), candidates_.end());
    for (const auto& directory : directories_) {
        for (const auto& set : direct_media_set_manifest()) {
            try {
                static_cast<void>(verify_direct_set(directory, set, false));
                ++report_.verified_direct_set_occurrences;
                const auto existing = std::find_if(releases_.begin(), releases_.end(), [&set](const auto& release) {
                    return release.sha256 == set.content_release_sha256;
                });
                if (existing == releases_.end()) {
                    releases_.push_back({set.game, set.platform, std::string(set.language),
                        std::string(set.content_release_sha256), directory,
                        ReleaseMediaLayout::verified_directory});
                    for (const auto& member : set.members) {
                        bound_direct_media_paths_.push_back(directory / std::string(member.name));
                    }
                } else {
                    ++report_.duplicate_direct_set_occurrences;
                }
            } catch (const std::exception&) {
                // An incomplete or changed directory remains ordinary direct
                // evidence only; its individual leaves are handled below.
            }
        }
    }
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
            const bool outer_size_matches = std::any_of(manifest.begin(), manifest.end(),
                [size](const auto& known) { return known.size == size; });
            const bool direct_size_matches = has_manifest_leaf_size(size);
            if (!outer_size_matches && !direct_size_matches) {
                ++report_.size_rejected_candidates;
                continue;
            }
            if (outer_size_matches) ++report_.size_candidates;
            if (direct_size_matches) ++report_.direct_media_size_candidates;
            const auto fingerprint = to_hex(sha256_file(candidate));
            ++report_.hashed_candidates;
            if (direct_size_matches) ++report_.direct_media_hashed_candidates;
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
            } else if (is_manifest_leaf(fingerprint, size)
                       && !is_bound_direct_path(bound_direct_media_paths_, candidate)) {
                ++report_.verified_direct_media_occurrences;
                const auto existing = std::find_if(unbound_direct_media_.begin(),
                    unbound_direct_media_.end(), [&fingerprint](const auto& media) {
                        return media.sha256 == fingerprint;
                    });
                if (existing == unbound_direct_media_.end()) {
                    unbound_direct_media_.push_back({fingerprint,
                        static_cast<std::uint64_t>(size), candidate});
                } else {
                    ++report_.duplicate_direct_media_occurrences;
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
    std::sort(unbound_direct_media_.begin(), unbound_direct_media_.end(),
        [](const auto& left, const auto& right) { return left.sha256 < right.sha256; });
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
