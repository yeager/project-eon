#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace eon {

enum class Game { millennium, deuteros };
enum class Platform { dos, amiga, atari_st };

struct ReleaseArchive {
    Game game;
    Platform platform;
    std::filesystem::path path;
};

[[nodiscard]] std::vector<ReleaseArchive> find_release_archives(
    const std::filesystem::path& directory);
[[nodiscard]] std::string name(Game game);
[[nodiscard]] std::string name(Platform platform);

} // namespace eon

