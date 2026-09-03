#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace eon {

// User-owned renderer settings. These numbers deliberately describe only
// Project Eon's own UI controls; they contain neither game paths nor original
// media, save data, guest memory, or recovered simulation state.
struct PresentationPreferences {
    std::size_t output_resolution_index = 0;
    std::size_t aspect_ratio_index = 0;
    std::size_t modern_preset_index = 0;
    std::size_t render_pacing_index = 0;
    // 0=off, 1=Scale2x, 2=Scale4x. This remains host presentation state.
    std::size_t pixel_reconstruction_index = 1;
    bool smooth_scaling = true;
    bool scanlines = false;
    bool frame = true;
    // Suppresses Eon's own animated/ornamental Modern overlay. It has no
    // connection to original game timing, media, controls or input polling.
    bool reduced_motion = false;
    // Launcher chrome only. This is intentionally distinct from the selected
    // original-release language and contains no original game text.
    std::string launcher_language = "en";
};

[[nodiscard]] std::filesystem::path default_presentation_preferences_path();
[[nodiscard]] std::optional<PresentationPreferences> load_presentation_preferences(
    const std::filesystem::path& path);
// A save is called only after the user confirms a renderer modal. It creates
// its dedicated parent directory but never creates, inspects, or modifies a
// supplied data directory or original save/media file.
[[nodiscard]] bool save_presentation_preferences(const std::filesystem::path& path,
    const PresentationPreferences& preferences);

// Persist one deliberate launcher-language choice while retaining the current
// renderer-only settings. This is called only from Eon's card menu; it never
// looks up, creates, or modifies game-data/media directories.
[[nodiscard]] bool save_launcher_language_preference(const std::filesystem::path& path,
    std::string_view language);

} // namespace eon
