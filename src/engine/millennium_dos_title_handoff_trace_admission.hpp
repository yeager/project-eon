#pragma once

#include "engine/millennium_dos_title_to_game_session.hpp"

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace eon {

// A completed, value-only native title-to-game boundary. It proves only the
// documented request for 2200AD.EXE; it neither starts that program nor
// emulates DOS, title input, rendering, audio, or gameplay.
struct MillenniumDosTitleHandoffTraceAdmission {
    std::optional<MillenniumDosTitleToGameSession> session;
    std::string error;
};

// Replays the strict external eight-record schema into a fresh native state
// machine. The caller must first pin the trace manifest and media identity.
[[nodiscard]] MillenniumDosTitleHandoffTraceAdmission
admit_millennium_dos_title_handoff_trace(std::span<const std::uint8_t> mill_launcher,
    std::span<const std::uint8_t> titles_executable, std::string_view events);

} // namespace eon
