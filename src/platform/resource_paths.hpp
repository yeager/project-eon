#pragma once

#include <array>
#include <filesystem>

namespace eon {

// Candidate locations used by relocated desktop packages. SDL_GetBasePath()
// returns Contents/Resources by default for macOS/iOS bundles, while the
// desktop macOS packager stores renderer assets beside the executable in
// Contents/MacOS/assets. A developer build may additionally opt into
// source-tree fallbacks, but installed packages must resolve their own
// resources without a checkout present.
[[nodiscard]] std::array<std::filesystem::path, 6> launcher_asset_paths(
    const std::filesystem::path& executable_path,
    const std::filesystem::path& category,
    const std::filesystem::path& filename,
    const std::filesystem::path& source_root = {});

[[nodiscard]] std::array<std::filesystem::path, 6> launcher_font_paths(
    const std::filesystem::path& executable_path,
    const std::filesystem::path& source_root = {});

[[nodiscard]] std::array<std::filesystem::path, 4> launcher_locale_paths(
    const std::filesystem::path& executable_path);

} // namespace eon
