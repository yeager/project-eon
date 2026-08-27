#include "platform/game_data.hpp"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace eon {
namespace {

std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return text;
}

} // namespace

std::vector<ReleaseArchive> find_release_archives(const std::filesystem::path& directory) {
    std::vector<ReleaseArchive> releases;
    std::error_code error;
    for (std::filesystem::directory_iterator iterator(directory, error), end;
         !error && iterator != end; iterator.increment(error)) {
        if (!iterator->is_regular_file() || lower(iterator->path().extension().string()) != ".zip") {
            continue;
        }
        const auto filename = lower(iterator->path().filename().string());
        const bool millennium = filename.find("millennium-return-to-earth") != std::string::npos;
        const bool deuteros = filename.find("deuteros-the-next-millennium") != std::string::npos;
        if (!millennium && !deuteros) {
            continue;
        }
        Platform platform;
        if (filename.find("_amiga_") != std::string::npos) {
            platform = Platform::amiga;
        } else if (filename.find("_atari-st_") != std::string::npos) {
            platform = Platform::atari_st;
        } else if (filename.find("_dos_") != std::string::npos) {
            platform = Platform::dos;
        } else {
            continue;
        }
        releases.push_back({millennium ? Game::millennium : Game::deuteros, platform, iterator->path()});
    }
    std::sort(releases.begin(), releases.end(), [](const auto& left, const auto& right) {
        return left.path < right.path;
    });
    return releases;
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

