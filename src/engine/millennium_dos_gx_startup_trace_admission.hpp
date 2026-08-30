#pragma once

#include "engine/millennium_dos_gx_startup_session.hpp"

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace eon {

// Result of admitting a complete, independently pinned GX-startup capture.
// The session represents only the call-free overlay writes proven by those
// ten observations.  It never emulates DOS, the private interrupt, the six
// opaque local callees, title input, or a frame.
struct MillenniumDosGxStartupTraceAdmission {
    std::optional<MillenniumDosGxStartupSession> session;
    std::string error;
};

// Construct and advance a fresh session from the strict ten-record grammar.
// The caller is responsible for first validating a user-supplied manifest via
// validate_reference_trace, including archive identity and event-file hash.
[[nodiscard]] MillenniumDosGxStartupTraceAdmission admit_millennium_dos_gx_startup_trace(
    std::span<const std::uint8_t> game_executable,
    std::span<const std::uint8_t> gx_overlay_executable,
    std::string_view events);

} // namespace eon
