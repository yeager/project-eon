#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace eon {

// This adapter preserves declared observations from exactly the hash-pinned
// Deuteros Atari ST boot boundary.  It is a schema validator only: no event
// becomes an XBIOS call, a callback frame, a dispatch decision, or a replay.
struct DeuterosAtariReferenceTraceDiagnostics {
    std::size_t event_count = 0;
    std::size_t trap_count = 0;
    std::size_t callback_count = 0;
    std::size_t frame_count = 0;
    std::size_t state_count = 0;
    std::size_t table_count = 0;
    std::size_t raw_reader_count = 0;
};

// Validate only the acquisition observations documented in PRESERVATION.md.
// The external stream must already have been bounded and hash-pinned by the
// generic reference-trace validator.  Accepted values are retained as
// provenance counts only and are never exposed as runtime inputs.
[[nodiscard]] bool validate_deuteros_atari_reference_events(
    std::string_view events, DeuterosAtariReferenceTraceDiagnostics& diagnostics,
    std::string& error);

} // namespace eon
