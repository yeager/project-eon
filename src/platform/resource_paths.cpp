#include "platform/resource_paths.hpp"

namespace eon {
namespace {

std::filesystem::path launcher_base_directory(const std::filesystem::path& path) {
    auto base = std::filesystem::is_directory(path) ? path : path.parent_path();
    // SDL_GetBasePath guarantees a trailing separator. Its final parent_path()
    // removes that separator rather than returning the containing directory,
    // so strip it before deriving Contents/MacOS from Contents/Resources.
    while (!base.empty() && base.filename().empty() && base != base.root_path()) {
        base = base.parent_path();
    }
    return base;
}

} // namespace

std::array<std::filesystem::path, 6> launcher_asset_paths(
    const std::filesystem::path& executable_path,
    const std::filesystem::path& category,
    const std::filesystem::path& filename,
    const std::filesystem::path& source_root) {
    const auto base = launcher_base_directory(executable_path);
    const auto bundle_resources = base.parent_path() / "Resources";
    return {{
        base / "assets" / category / filename,
        base.parent_path() / "MacOS" / "assets" / category / filename,
        bundle_resources / "assets" / category / filename,
        base / "../share/project-eon/assets" / category / filename,
        !source_root.empty() ? source_root / "assets" / category / filename
                                : std::filesystem::path{},
        std::filesystem::path("assets") / category / filename,
    }};
}

std::array<std::filesystem::path, 6> launcher_font_paths(
    const std::filesystem::path& executable_path,
    const std::filesystem::path& source_root) {
    const auto base = launcher_base_directory(executable_path);
    const auto bundle_resources = base.parent_path() / "Resources";
    return {{
        base / "assets" / "fonts",
        base.parent_path() / "MacOS" / "assets" / "fonts",
        bundle_resources / "assets" / "fonts",
        base / "../share/project-eon/assets/fonts",
        !source_root.empty() ? source_root / "assets" / "fonts"
                                : std::filesystem::path{},
        std::filesystem::path("assets") / "fonts",
    }};
}

std::array<std::filesystem::path, 4> launcher_locale_paths(
    const std::filesystem::path& executable_path) {
    const auto base = launcher_base_directory(executable_path);
    const auto bundle_resources = base.parent_path() / "Resources";
    return {{
        base / "po",
        base / "../share/project-eon/po",
        bundle_resources / "po",
        base / "Resources" / "po",
    }};
}

} // namespace eon
