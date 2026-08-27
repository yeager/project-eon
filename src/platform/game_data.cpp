#include "platform/game_data.hpp"

#include "data/sha256.hpp"

#include <algorithm>
#include <array>
#include <exception>
#include <system_error>

namespace eon {
namespace {

struct KnownRelease {
    const char* sha256;
    Game game;
    Platform platform;
    const char* language;
    std::uintmax_t size;
};

// Fingerprints are calculated from the genuine outer release archives in the
// supplied corpus. Names and directory layout are deliberately not trusted.
constexpr std::array<KnownRelease, 6> known_releases{{
    {"f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04", Game::deuteros, Platform::amiga, "en", 4'066'771},
    {"c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653", Game::deuteros, Platform::atari_st, "en", 3'021'682},
    {"2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400", Game::millennium, Platform::amiga, "en", 2'558'009},
    {"ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01", Game::millennium, Platform::atari_st, "en", 1'524'836},
    {"e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", Game::millennium, Platform::dos, "en", 328'383},
    {"b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4", Game::millennium, Platform::dos, "es", 330'050},
}};

} // namespace

std::vector<ReleaseArchive> find_release_archives(const std::filesystem::path& directory) {
    ReleaseScanner scanner(directory);
    while (!scanner.advance(64)) {
    }
    return scanner.releases();
}

ReleaseScanner::ReleaseScanner(const std::filesystem::path& directory) {
    std::error_code error;
    for (std::filesystem::recursive_directory_iterator iterator(
             directory, std::filesystem::directory_options::skip_permission_denied, error), end;
         !error && iterator != end; iterator.increment(error)) {
        std::error_code file_error;
        if (!iterator->is_regular_file(file_error) || file_error) continue;
        candidates_.push_back(iterator->path());
    }
    std::sort(candidates_.begin(), candidates_.end());
}

bool ReleaseScanner::advance(std::size_t max_files) {
    const auto until = std::min(candidates_.size(), next_candidate_ + max_files);
    while (next_candidate_ < until) {
        const auto& candidate = candidates_[next_candidate_++];
        try {
            const auto size = std::filesystem::file_size(candidate);
            if (std::none_of(known_releases.begin(), known_releases.end(),
                    [size](const auto& known) { return known.size == size; })) continue;
            const auto fingerprint = to_hex(sha256_file(candidate));
            const auto found = std::find_if(known_releases.begin(), known_releases.end(),
                [&fingerprint](const auto& known) { return fingerprint == known.sha256; });
            if (found != known_releases.end()) {
                releases_.push_back({found->game, found->platform, found->language,
                    fingerprint, candidate});
            }
        } catch (const std::exception&) {
            // A file may disappear during a scan. It is simply not a verified
            // release; no filename fallback is allowed.
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
