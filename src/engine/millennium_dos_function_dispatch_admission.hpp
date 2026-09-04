#pragma once

#include "engine/millennium_dos_post_overlay_loop_session.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace eon {

// Independent observation of the original scaled dispatcher. None of these
// values is derived or defaulted by the admission gate.
struct MillenniumDosFunctionDispatchObservation {
    std::uint16_t scaled_call_address = 0;
    std::uint16_t dispatcher_address = 0;
    std::size_t function_key_index = 0;
    std::uint16_t handler_address = 0;
};

// Value-only result. Acceptance proves internal agreement between exact
// executable parsing, the terminal loop boundary and the explicit observer;
// it does not prove that a complete live launch reached that loop.
struct MillenniumDosFunctionDispatchAdmission {
    bool accepted = false;
    std::size_t function_key_index = 0;
    std::uint16_t handler_address = 0;
    std::string error;
};

[[nodiscard]] MillenniumDosFunctionDispatchAdmission
admit_millennium_dos_function_dispatch(
    std::span<const std::uint8_t> game_executable,
    const MillenniumDosPostOverlayLoopSession& loop,
    MillenniumDosFunctionDispatchObservation observation);

} // namespace eon
