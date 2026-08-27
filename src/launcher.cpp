#include "launcher.hpp"

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

} // namespace

std::string usage() {
    return
        "Usage:\n"
        "  project-eon --data <directory>\n"
        "  project-eon --data <directory> --game millennium|deuteros\n"
        "               [--platform dos|amiga|atari-st]\n"
        "               [--presentation original|modern]\n\n"
        "  project-eon --data <directory> --verify-data millennium|deuteros\n\n"
        "Without --game, the graphical start menu is shown.\n";
}

ParseResult parse_command_line(int argc, char** argv) {
    LaunchRequest request;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--help" || argument == "-h") return {{}, {}, true};
        if (index + 1 >= argc) return {{}, "Missing value for " + std::string(argument), false};
        const std::string_view value = argv[++index];
        if (argument == "--data") {
            request.data_directory = value;
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
    if (request.data_directory.empty()) return {{}, "--data is required", false};
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
