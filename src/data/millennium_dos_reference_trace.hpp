#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace eon {

// This adapter accepts declared observations from exactly the clean English
// Millennium DOS startup path. It is deliberately a validator, not a guest
// execution surface: accepted observations are reported as provenance only.
struct MillenniumDosEnglishReferenceTraceDiagnostics {
    std::size_t event_count = 0;
    std::size_t interrupt_count = 0;
    std::size_t file_count = 0;
    std::size_t exec_count = 0;
};

// Parse the v2 event grammar after the generic trace validator has pinned its
// external file by size and SHA-256. The caller provides the whole UTF-8/ASCII
// text only for this bounded, diagnostics-only validation; no event returns
// an emulated result or changes runtime state.
[[nodiscard]] bool validate_millennium_dos_english_reference_events(
    std::string_view events,
    MillenniumDosEnglishReferenceTraceDiagnostics& diagnostics,
    std::string& error);

} // namespace eon
