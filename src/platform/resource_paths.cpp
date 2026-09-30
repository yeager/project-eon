#include "platform/resource_paths.hpp"

namespace eon {

std::array<std::filesystem::path, 5> launcher_asset_paths(
    const std::filesystem::path& executable_path,
    const std::filesystem::path& category,
    const std::filesystem::path& filename,
    const std::filesystem::path& source_root) {
    const auto base = executable_path.parent_path();
    const auto bundle_resources = base.parent_path() / "Resources";
    return {{
        base / "assets" / category / filename,
        bundle_resources / "assets" / category / filename,
        base / "../share/project-eon/assets" / category / filename,
        !source_root.empty() ? source_root / "assets" / category / filename
                                : std::filesystem::path{},
        std::filesystem::path("assets") / category / filename,
    }};
}

std::array<std::filesystem::path, 5> launcher_font_paths(
    const std::filesystem::path& executable_path,
    const std::filesystem::path& source_root) {
    const auto base = executable_path.parent_path();
    const auto bundle_resources = base.parent_path() / "Resources";
    return {{
        base / "assets" / "fonts",
        bundle_resources / "assets" / "fonts",
        base / "../share/project-eon/assets/fonts",
        !source_root.empty() ? source_root / "assets" / "fonts"
                                : std::filesystem::path{},
        std::filesystem::path("assets") / "fonts",
    }};
}

std::array<std::filesystem::path, 4> launcher_locale_paths(
    const std::filesystem::path& executable_path) {
    const auto base = executable_path.parent_path();
    const auto bundle_resources = base.parent_path() / "Resources";
    return {{
        base / "po",
        base / "../share/project-eon/po",
        bundle_resources / "po",
        base / "Resources" / "po",
    }};
}

} // namespace eon
