#include "launcher.hpp"
#include "i18n.hpp"

#include <algorithm>
#include <cstdlib>
#include <string_view>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

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

bool parse_display_resolution(const std::string_view value, DisplayPreferences& display) {
    if (value == "1280x720") display = {1280, 720, display.aspect_ratio_index};
    else if (value == "1600x900") display = {1600, 900, display.aspect_ratio_index};
    else if (value == "1920x1080") display = {1920, 1080, display.aspect_ratio_index};
    else return false;
    return true;
}

bool parse_display_aspect(const std::string_view value, DisplayPreferences& display) {
    if (value == "original") display.aspect_ratio_index = 0;
    else if (value == "square-pixels") display.aspect_ratio_index = 1;
    else if (value == "widescreen") display.aspect_ratio_index = 2;
    else return false;
    return true;
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
#if defined(__APPLE__) && TARGET_OS_IPHONE
        // IPA payloads deliberately contain no game data. iPadOS therefore
        // uses a Files-visible, non-hidden user-media location, without
        // creating it during this read-only lookup.
        return std::filesystem::path(home) / "Documents" / "ProjectEon";
#else
        return std::filesystem::path(home) / ".projecteon";
#endif
    }
#if defined(__APPLE__) && TARGET_OS_IPHONE
    return std::filesystem::path("Documents") / "ProjectEon";
#else
    return std::filesystem::path(".projecteon");
#endif
#endif
}

} // namespace

std::string usage() {
    return
        "Usage:\n"
        "  project-eon [--data|--data-dir <directory-or-archive>]\n"
        "  project-eon [--data|--data-dir <directory-or-archive>] --game millennium|deuteros\n"
        "               --platform dos|amiga|atari-st\n"
        "               [--presentation original|modern] [--modern-pack <pack.eonmodern>]\n\n"
        "               [--resolution 1280x720|1600x900|1920x1080]\n"
        "               [--aspect original|square-pixels|widescreen]\n\n"
        "               [--language <language>]\n\n"
        "  project-eon [--data|--data-dir <directory-or-archive>] --verify-data millennium|deuteros\n\n"
        "  project-eon --data <directory-or-archive> --game millennium|deuteros\n"
        "               --platform dos|amiga|atari-st --reference-trace <manifest.eontrace>\n\n"
        "  project-eon [--data|--data-dir <directory-or-archive>] --inspect\n"
        "               [--game millennium|deuteros] [--platform dos|amiga|atari-st]\n"
        "               [--modern-packs <explicit-pack-root>]\n\n"
#if defined(__APPLE__) && TARGET_OS_IPHONE
        "Without --data/--data-dir, iPadOS reads user-supplied media from Documents/ProjectEon.\n"
#else
        "Without --data/--data-dir, game data is read from ~/.projecteon on Linux/macOS\n"
        "or <install directory>/data on Windows.\n"
#endif
        "Without --game, the graphical\n"
        "start menu is shown.\n";
}

ParseResult parse_command_line(int argc, char** argv) {
    LaunchRequest request;
    request.data_directory = default_data_directory(argc > 0 ? argv[0] : "project-eon");
    request.language = language_from_environment();
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--help" || argument == "-h") return {{}, {}, true};
        if (argument == "--inspect") {
            request.inspect_data = true;
            continue;
        }
        if (index + 1 >= argc) return {{}, "Missing value for " + std::string(argument), false};
        const std::string_view value = argv[++index];
        if (argument == "--data" || argument == "--data-dir") {
            request.data_directory = value;
            request.data_directory_is_default = false;
        } else if (argument == "--game") {
            request.game = parse_game(value);
            if (!request.game) return {{}, "Unknown game: " + std::string(value), false};
        } else if (argument == "--verify-data") {
            request.verify_game = parse_game(value);
            if (!request.verify_game) return {{}, "Unknown game: " + std::string(value), false};
        } else if (argument == "--reference-trace") {
            request.reference_trace = std::filesystem::path(value);
        } else if (argument == "--modern-packs") {
            request.modern_pack_root = std::filesystem::path(value);
        } else if (argument == "--modern-pack") {
            request.modern_pack_manifest = std::filesystem::path(value);
        } else if (argument == "--platform") {
            request.platform = parse_platform(value);
            if (!request.platform) return {{}, "Unknown platform: " + std::string(value), false};
        } else if (argument == "--presentation") {
            if (value == "original") request.presentation = Presentation::original;
            else if (value == "modern") request.presentation = Presentation::modern;
            else return {{}, "Unknown presentation: " + std::string(value), false};
        } else if (argument == "--resolution") {
            if (!parse_display_resolution(value, request.display)) {
                return {{}, "Unsupported resolution: " + std::string(value), false};
            }
        } else if (argument == "--aspect") {
            if (!parse_display_aspect(value, request.display)) {
                return {{}, "Unknown aspect ratio: " + std::string(value), false};
            }
        } else if (argument == "--language" || argument == "-l") {
            request.language = normalize_language(value);
            if (request.language.empty()) return {{}, "Unknown language: " + std::string(value), false};
        } else {
            return {{}, "Unknown option: " + std::string(argument), false};
        }
    }
    if (request.game && request.verify_game) {
        return {{}, "--game and --verify-data cannot be combined", false};
    }
    if (request.verify_game && request.inspect_data) {
        return {{}, "--verify-data and --inspect cannot be combined", false};
    }
    if (request.modern_pack_root && !request.inspect_data) {
        return {{}, "--modern-packs requires --inspect; it is diagnostics-only and never selects a renderer pack", false};
    }
    if (request.modern_pack_manifest && (request.inspect_data || request.presentation != Presentation::modern
        || !request.game || !request.platform)) {
        return {{}, "--modern-pack requires --game, --platform, and --presentation modern; it cannot be used with --inspect", false};
    }
    if (request.reference_trace) {
        if (request.data_directory_is_default) {
            return {{}, "--reference-trace requires an explicit --data or --data-dir path", false};
        }
        if (!request.game || !request.platform) {
            return {{}, "--reference-trace requires both --game and --platform", false};
        }
        if (request.verify_game || request.inspect_data) {
            return {{}, "--reference-trace cannot be combined with --verify-data or --inspect", false};
        }
    }
    if (request.platform && !request.game) return {{}, "--platform requires --game", false};
    // The graphical flow records a platform card before a profile can start
    // a game. A direct CLI launch needs the same unambiguous release choice:
    // never let an omitted flag choose a different platform's recovered path.
    // Inspection remains a filter and therefore may name a game alone.
    if (request.game && !request.inspect_data && !request.platform) {
        return {{}, "--game requires --platform for a direct launch; use --inspect to list verified platforms", false};
    }
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

std::vector<Platform> available_platforms(
    const std::vector<ReleaseArchive>& releases, const Game game) {
    std::vector<Platform> platforms;
    for (const auto platform : {Platform::dos, Platform::amiga, Platform::atari_st}) {
        if (release_available(releases, game, platform)) platforms.push_back(platform);
    }
    return platforms;
}

std::optional<Platform> select_available_platform(
    const std::vector<ReleaseArchive>& releases, const Game game,
    const std::optional<Platform> current) {
    const auto platforms = available_platforms(releases, game);
    if (platforms.empty()) return std::nullopt;
    if (current && std::find(platforms.begin(), platforms.end(), *current) != platforms.end()) {
        return current;
    }
    return platforms.front();
}

bool deuteros_amiga_opening_supported(std::optional<Platform> platform) {
    return !platform || *platform == Platform::amiga;
}

} // namespace eon
