#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>

namespace eon {

// User-owned renderer settings. These numbers deliberately describe only
// Project Eon's own UI controls; they contain neither game paths nor original
// media, save data, guest memory, or recovered simulation state.
struct PresentationPreferences {
    std::size_t output_resolution_index = 0;
    std::size_t aspect_ratio_index = 0;
    std::size_t modern_preset_index = 0;
    std::size_t render_pacing_index = 0;
    bool pixel_reconstruction = true;
    bool smooth_scaling = true;
    bool scanlines = false;
    bool frame = true;
};

[[nodiscard]] std::filesystem::path default_presentation_preferences_path();
[[nodiscard]] std::optional<PresentationPreferences> load_presentation_preferences(
    const std::filesystem::path& path);
// A save is called only after the user confirms a renderer modal. It creates
// its dedicated parent directory but never creates, inspects, or modifies a
// supplied data directory or original save/media file.
[[nodiscard]] bool save_presentation_preferences(const std::filesystem::path& path,
    const PresentationPreferences& preferences);

} // namespace eon
