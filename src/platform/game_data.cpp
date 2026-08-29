#include "platform/game_data.hpp"

#include "data/release_manifest.hpp"
#include "data/sha256.hpp"

#include <algorithm>
#include <exception>
#include <system_error>

namespace eon {
namespace {

} // namespace

std::vector<ReleaseArchive> find_release_archives(const std::filesystem::path& directory) {
    ReleaseScanner scanner(directory);
    while (!scanner.advance(64)) {
    }
    return scanner.releases();
}

ReleaseScanner::ReleaseScanner(const std::filesystem::path& directory) {
    std::error_code error;
    if (std::filesystem::is_regular_file(directory, error) && !error) {
        candidates_.push_back(directory);
        report_.candidates = candidates_.size();
        return;
    }
    error.clear();
    for (std::filesystem::recursive_directory_iterator iterator(
             directory, std::filesystem::directory_options::skip_permission_denied, error), end;
         !error && iterator != end; iterator.increment(error)) {
        std::error_code file_error;
        if (!iterator->is_regular_file(file_error) || file_error) continue;
        candidates_.push_back(iterator->path());
    }
    std::sort(candidates_.begin(), candidates_.end());
    report_.candidates = candidates_.size();
}

bool ReleaseScanner::advance(std::size_t max_files) {
    const auto until = std::min(candidates_.size(), next_candidate_ + max_files);
    while (next_candidate_ < until) {
        const auto& candidate = candidates_[next_candidate_++];
        try {
            const auto size = std::filesystem::file_size(candidate);
            const auto manifest = release_manifest();
            if (std::none_of(manifest.begin(), manifest.end(),
                    [size](const auto& known) { return known.size == size; })) continue;
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
