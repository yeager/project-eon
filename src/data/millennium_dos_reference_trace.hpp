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

// The GX continuation has a separate event grammar because it records return
// observations that the earlier launcher/startup schema intentionally does
// not admit.  The values remain opaque capture provenance: validation never
// feeds them into a session or emulates a DOS/private-interrupt result.
struct MillenniumDosGxStartupReferenceTraceDiagnostics {
    std::size_t event_count = 0;
    std::size_t private_return_count = 0;
    std::size_t mode_read_count = 0;
    std::size_t adapter_return_count = 0;
    std::size_t local_return_count = 0;
};

// Parse the v2 event grammar after the generic trace validator has pinned its
// external file by size and SHA-256. The caller provides the whole UTF-8/ASCII
// text only for this bounded, diagnostics-only validation; no event returns
// an emulated result or changes runtime state.
[[nodiscard]] bool validate_millennium_dos_english_reference_events(
    std::string_view events,
    MillenniumDosEnglishReferenceTraceDiagnostics& diagnostics,
    std::string& error);

[[nodiscard]] bool validate_millennium_dos_gx_startup_reference_events(
    std::string_view events,
    MillenniumDosGxStartupReferenceTraceDiagnostics& diagnostics,
    std::string& error);

} // namespace eon
