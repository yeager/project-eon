#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace eon {

// Capture-only contract for the first observed Deuteros Amiga title display.
// It intentionally contains no replay values or emulated ABI behaviour.
inline constexpr std::string_view deuteros_amiga_title_display_reference_trace_adapter =
    "project-eon-reference-trace-v4/deuteros-amiga-en-title-display-v4";

struct DeuterosAmigaTitleDisplayReferenceTraceDiagnostics {
    std::size_t event_count = 0;
    std::size_t bridge_event_count = 0;
    std::size_t display_layout_count = 0;
    std::size_t bitplane_layout_count = 0;
    std::size_t palette_checkpoint_count = 0;
    std::size_t input_checkpoint_count = 0;
    std::size_t frame_checkpoint_count = 0;
    std::size_t audio_checkpoint_count = 0;
    // These remain opaque provenance values. A v4 trace merely records them;
    // v5 admission cross-checks them against separately hash-verified capture
    // artifacts without exposing any bytes to a runtime session.
    std::string copper_list_sha256;
    std::string rgb4_palette_sha256;
    std::string rgba_palette_sha256;
    std::string bitplanes_sha256;
    std::string rgba_sha256;
    std::string audio_sample_rate;
    std::string audio_channels;
    std::string audio_sample_frames;
    std::string pcm_sha256;
};

// Requires a complete v3 ordered bridge prefix followed by one ordered,
// raw display checkpoint sequence. Validation is diagnostic/admission only.
[[nodiscard]] bool validate_deuteros_amiga_title_display_reference_events(
    std::string_view events, DeuterosAmigaTitleDisplayReferenceTraceDiagnostics& diagnostics,
    std::string& error, std::string_view expected_input_timeline_sha256 = {});

} // namespace eon
