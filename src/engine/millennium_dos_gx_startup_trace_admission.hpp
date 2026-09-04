#pragma once

#include "engine/millennium_dos_gx_startup_session.hpp"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace eon {

// Result of admitting a complete, independently pinned GX-startup capture.
// The session represents only the call-free overlay writes proven by those
// ten observations.  It never emulates DOS, the private interrupt, the six
// opaque local callees, title input, or a frame.
class MillenniumDosGxStartupTraceAdmission {
private:
    // `MillenniumDosGxStartupSession` validates through spans. Retain exact
    // private copies for the complete admission lifetime; declaration order
    // guarantees `session` is destroyed before either backing buffer.
    std::vector<std::uint8_t> game_executable_;
    std::vector<std::uint8_t> gx_overlay_executable_;
    friend MillenniumDosGxStartupTraceAdmission admit_millennium_dos_gx_startup_trace(
        std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::string_view);

public:
    MillenniumDosGxStartupTraceAdmission() = default;
    MillenniumDosGxStartupTraceAdmission(MillenniumDosGxStartupTraceAdmission&&) = default;
    MillenniumDosGxStartupTraceAdmission& operator=(MillenniumDosGxStartupTraceAdmission&&) = default;
    MillenniumDosGxStartupTraceAdmission(const MillenniumDosGxStartupTraceAdmission&) = delete;
    MillenniumDosGxStartupTraceAdmission& operator=(
        const MillenniumDosGxStartupTraceAdmission&) = delete;
    ~MillenniumDosGxStartupTraceAdmission() = default;
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
