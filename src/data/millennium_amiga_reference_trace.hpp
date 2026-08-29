#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace eon {

// Validation for the exact caller-side handoffs in the clean English
// Millennium Amiga Defjam bootstrap.  This consumes external observations as
// provenance only.  It does not execute an instruction, map the opaque
// source stage into RAM, or infer a call result or return path.
struct MillenniumAmigaReferenceTraceDiagnostics {
    std::size_t event_count = 0;
    std::size_t cpu_count = 0;
};

[[nodiscard]] bool validate_millennium_amiga_english_reference_events(
    std::string_view events, MillenniumAmigaReferenceTraceDiagnostics& diagnostics,
    std::string& error);

} // namespace eon
