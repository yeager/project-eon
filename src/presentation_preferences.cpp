#include "presentation_preferences.hpp"

#include <cstdlib>
#include <fstream>
#include <string>
#include <string_view>

namespace eon {
namespace {

constexpr std::string_view header = "project-eon-presentation-preferences=1";

std::optional<bool> parse_bool(const std::string_view value) {
    if (value == "0") return false;
    if (value == "1") return true;
    return std::nullopt;
}

std::optional<std::size_t> parse_index(const std::string_view value, const std::size_t upper_bound) {
    if (value.empty()) return std::nullopt;
    std::size_t result = 0;
    for (const auto character : value) {
        if (character < '0' || character > '9') return std::nullopt;
        if (result > upper_bound / 10U) return std::nullopt;
        result = result * 10U + static_cast<std::size_t>(character - '0');
        if (result > upper_bound) return std::nullopt;
    }
    return result;
}

} // namespace

std::filesystem::path default_presentation_preferences_path() {
#ifdef _WIN32
    if (const auto* appdata = std::getenv("APPDATA"); appdata && *appdata) {
        return std::filesystem::path(appdata) / "ProjectEon" / "presentation-v1.ini";
    }
    return std::filesystem::path("ProjectEon") / "presentation-v1.ini";
#elif defined(__APPLE__)
    if (const auto* home = std::getenv("HOME"); home && *home) {
        return std::filesystem::path(home) / "Library" / "Application Support" / "Project Eon"
            / "presentation-v1.ini";
    }
    return std::filesystem::path("Project Eon") / "presentation-v1.ini";
#else
    if (const auto* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) {
        return std::filesystem::path(xdg) / "project-eon" / "presentation-v1.ini";
    }
    if (const auto* home = std::getenv("HOME"); home && *home) {
        return std::filesystem::path(home) / ".config" / "project-eon" / "presentation-v1.ini";
    }
    return std::filesystem::path(".config") / "project-eon" / "presentation-v1.ini";
#endif
}

std::optional<PresentationPreferences> load_presentation_preferences(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return std::nullopt;
    std::string line;
    if (!std::getline(input, line) || line != header) return std::nullopt;
    PresentationPreferences preferences;
    bool resolution = false, aspect = false, preset = false, pacing = false;
    bool reconstruction = false, scaling = false, scanlines = false, frame = false;
    while (std::getline(input, line)) {
        const auto separator = line.find('=');
        if (separator == std::string::npos) return std::nullopt;
        const std::string_view key(line.data(), separator);
        const std::string_view value(line.data() + separator + 1U, line.size() - separator - 1U);
        if (key == "resolution") { const auto parsed = parse_index(value, 2); if (!parsed || resolution) return std::nullopt; preferences.output_resolution_index = *parsed; resolution = true; }
        else if (key == "aspect") { const auto parsed = parse_index(value, 2); if (!parsed || aspect) return std::nullopt; preferences.aspect_ratio_index = *parsed; aspect = true; }
        else if (key == "preset") { const auto parsed = parse_index(value, 4); if (!parsed || preset) return std::nullopt; preferences.modern_preset_index = *parsed; preset = true; }
        else if (key == "pacing") { const auto parsed = parse_index(value, 2); if (!parsed || pacing) return std::nullopt; preferences.render_pacing_index = *parsed; pacing = true; }
        else if (key == "reconstruction") { const auto parsed = parse_index(value, 2); if (!parsed || reconstruction) return std::nullopt; preferences.pixel_reconstruction_index = *parsed; reconstruction = true; }
        else if (key == "scaling") { const auto parsed = parse_bool(value); if (!parsed || scaling) return std::nullopt; preferences.smooth_scaling = *parsed; scaling = true; }
        else if (key == "scanlines") { const auto parsed = parse_bool(value); if (!parsed || scanlines) return std::nullopt; preferences.scanlines = *parsed; scanlines = true; }
        else if (key == "frame") { const auto parsed = parse_bool(value); if (!parsed || frame) return std::nullopt; preferences.frame = *parsed; frame = true; }
        else return std::nullopt;
    }
    if (!resolution || !aspect || !preset || !pacing || !reconstruction || !scaling || !scanlines || !frame) return std::nullopt;
    return preferences;
}

bool save_presentation_preferences(const std::filesystem::path& path,
    const PresentationPreferences& preferences) {
    if (preferences.output_resolution_index > 2 || preferences.aspect_ratio_index > 2
        || preferences.modern_preset_index > 4 || preferences.render_pacing_index > 2) return false;
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return false;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output << header << '\n'
           << "resolution=" << preferences.output_resolution_index << '\n'
           << "aspect=" << preferences.aspect_ratio_index << '\n'
           << "preset=" << preferences.modern_preset_index << '\n'
           << "pacing=" << preferences.render_pacing_index << '\n'
           << "reconstruction=" << preferences.pixel_reconstruction_index << '\n'
           << "scaling=" << (preferences.smooth_scaling ? 1 : 0) << '\n'
           << "scanlines=" << (preferences.scanlines ? 1 : 0) << '\n'
           << "frame=" << (preferences.frame ? 1 : 0) << '\n';
    return static_cast<bool>(output);
}

} // namespace eon
