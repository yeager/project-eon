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
};

// Requires a complete v3 ordered bridge prefix followed by one ordered,
// raw display checkpoint sequence. Validation is diagnostic/admission only.
[[nodiscard]] bool validate_deuteros_amiga_title_display_reference_events(
    std::string_view events, DeuterosAmigaTitleDisplayReferenceTraceDiagnostics& diagnostics,
    std::string& error);

} // namespace eon
