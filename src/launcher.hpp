#pragma once

#include "platform/game_data.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace eon {

enum class Presentation { original, modern };

struct LaunchRequest {
    std::filesystem::path data_directory;
    bool data_directory_is_default = true;
    std::optional<Game> game;
    std::optional<Game> verify_game;
    bool inspect_data = false;
    std::optional<Platform> platform;
    Presentation presentation = Presentation::original;
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

// The recovered Deuteros SDL opening is backed only by the clean Amiga ADF.
// An omitted platform may select that verified preview from the menu, while
// an explicit Atari ST selection remains at the protected-boot boundary.
[[nodiscard]] bool deuteros_amiga_opening_supported(std::optional<Platform> platform);

} // namespace eon
