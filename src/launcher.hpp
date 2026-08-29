#pragma once

#include "platform/game_data.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

namespace eon {

enum class Presentation { original, modern };

// Renderer-only preferences shared by CLI startup and the F10 overlay. They
// never enter a game simulation, save file, or original media access path.
struct DisplayPreferences {
    int width = 1280;
    int height = 720;
    std::size_t aspect_ratio_index = 0; // Original 4:3, square 8:5, wide 16:9.
};

struct LaunchRequest {
    std::filesystem::path data_directory;
    bool data_directory_is_default = true;
    std::optional<Game> game;
    std::optional<Game> verify_game;
    std::optional<std::filesystem::path> reference_trace;
    bool inspect_data = false;
    std::optional<Platform> platform;
    Presentation presentation = Presentation::original;
    DisplayPreferences display;
    std::string language;
};

struct ParseResult {
    std::optional<LaunchRequest> request;
    std::string error;
    bool help = false;
};

[[nodiscard]] ParseResult parse_command_line(int argc, char** argv);
[[nodiscard]] std::string usage();
[[nodiscard]] bool release_available(
    const std::vector<ReleaseArchive>& releases,
    Game game,
    std::optional<Platform> platform);
// Ordered, hash-verified platforms that can be selected for one game. This is
// shared by the start menu and tests so a UI choice cannot silently fall back
// to another release.
[[nodiscard]] std::vector<Platform> available_platforms(
    const std::vector<ReleaseArchive>& releases, Game game);
// Retain a choice only when it belongs to the newly focused game. Otherwise
// choose that game's first hash-verified platform; no platform means no start.
[[nodiscard]] std::optional<Platform> select_available_platform(
    const std::vector<ReleaseArchive>& releases, Game game,
    std::optional<Platform> current);

// The recovered Deuteros SDL opening is backed only by the clean Amiga ADF.
// An omitted platform may select that verified preview from the menu, while
// an explicit Atari ST selection remains at the protected-boot boundary.
[[nodiscard]] bool deuteros_amiga_opening_supported(std::optional<Platform> platform);

} // namespace eon
