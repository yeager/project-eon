#include "launcher.hpp"

#include <cstdlib>
#include <string_view>

namespace eon {
namespace {

std::optional<Game> parse_game(std::string_view value) {
    if (value == "millennium") return Game::millennium;
    if (value == "deuteros") return Game::deuteros;
    return std::nullopt;
}

std::optional<Platform> parse_platform(std::string_view value) {
    if (value == "dos") return Platform::dos;
    if (value == "amiga") return Platform::amiga;
    if (value == "atari-st") return Platform::atari_st;
    return std::nullopt;
}

std::filesystem::path default_data_directory(const char* executable_path) {
#ifdef _WIN32
    std::error_code error;
    const auto absolute_executable = std::filesystem::absolute(executable_path, error);
    if (!error) return absolute_executable.parent_path() / "data";
    return std::filesystem::path("data");
#else
    static_cast<void>(executable_path);
    if (const auto* home = std::getenv("HOME"); home && *home) {
        return std::filesystem::path(home) / ".projecteon";
    }
    return std::filesystem::path(".projecteon");
#endif
}

} // namespace

std::string usage() {
    return
        "Usage:\n"
        "  project-eon [--data <directory>]\n"
        "  project-eon [--data <directory>] --game millennium|deuteros\n"
        "               [--platform dos|amiga|atari-st]\n"
        "               [--presentation original|modern]\n\n"
        "  project-eon [--data <directory>] --verify-data millennium|deuteros\n\n"
        "Without --data, game data is read from ~/.projecteon on Linux/macOS\n"
        "or <install directory>/data on Windows. Without --game, the graphical\n"
        "start menu is shown.\n";
}

ParseResult parse_command_line(int argc, char** argv) {
    LaunchRequest request;
    request.data_directory = default_data_directory(argc > 0 ? argv[0] : "project-eon");
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--help" || argument == "-h") return {{}, {}, true};
        if (index + 1 >= argc) return {{}, "Missing value for " + std::string(argument), false};
        const std::string_view value = argv[++index];
        if (argument == "--data") {
            request.data_directory = value;
            request.data_directory_is_default = false;
        } else if (argument == "--game") {
            request.game = parse_game(value);
            if (!request.game) return {{}, "Unknown game: " + std::string(value), false};
        } else if (argument == "--verify-data") {
            request.verify_game = parse_game(value);
            if (!request.verify_game) return {{}, "Unknown game: " + std::string(value), false};
        } else if (argument == "--platform") {
            request.platform = parse_platform(value);
            if (!request.platform) return {{}, "Unknown platform: " + std::string(value), false};
        } else if (argument == "--presentation") {
            if (value == "original") request.presentation = Presentation::original;
            else if (value == "modern") request.presentation = Presentation::modern;
            else return {{}, "Unknown presentation: " + std::string(value), false};
        } else {
            return {{}, "Unknown option: " + std::string(argument), false};
        }
    }
    if (request.game && request.verify_game) return {{}, "--game and --verify-data cannot be combined", false};
    if (request.platform && !request.game) return {{}, "--platform requires --game", false};
    return {request, {}, false};
}

bool release_available(
    const std::vector<ReleaseArchive>& releases,
    Game game,
    std::optional<Platform> platform) {
    for (const auto& release : releases) {
        if (release.game == game && (!platform || release.platform == *platform)) return true;
    }
    return false;
}

} // namespace eon
