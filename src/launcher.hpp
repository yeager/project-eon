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

} // namespace eon
