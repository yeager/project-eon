#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace eon {

// External, hash-pinned observations at the Deuteros Amiga title-stage ABI
// boundary.  These are provenance records only: validating one never calls
// Exec/graphics.library, writes a custom register, invokes a callback, or
// provides an observed result to the Eon runtime.
struct DeuterosAmigaReferenceTraceDiagnostics {
    std::size_t event_count = 0;
    std::size_t exec_count = 0;
    std::size_t open_library_count = 0;
    std::size_t graphics_count = 0;
    std::size_t custom_register_count = 0;
    std::size_t callback_count = 0;
};

[[nodiscard]] bool validate_deuteros_amiga_title_reference_events(
    std::string_view events, DeuterosAmigaReferenceTraceDiagnostics& diagnostics,
    std::string& error);

} // namespace eon
