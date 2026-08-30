#include "engine/millennium_dos_gx_startup_trace_admission.hpp"

#include "data/millennium_dos_reference_trace.hpp"

#include <stdexcept>

namespace eon {

MillenniumDosGxStartupTraceAdmission admit_millennium_dos_gx_startup_trace(
    const std::span<const std::uint8_t> game_executable,
    const std::span<const std::uint8_t> gx_overlay_executable,
    const std::string_view events) {
    MillenniumDosGxStartupTraceAdmission result;
    const auto observations = parse_millennium_dos_gx_startup_reference_observations(events, result.error);
    if (!observations) return result;
    try {
        MillenniumDosGxStartupSession session(game_executable, gx_overlay_executable);
        session.observe_private_return(observations->private_return_ax);
        session.observe_mode_byte(observations->initial_mode_byte);
        session.observe_adapter_return();
        for (std::size_t call = 0; call < 6; ++call) session.observe_post_overlay_call_return();
        session.observe_post_overlay_mode_byte(observations->post_overlay_mode_byte);
        if (session.state() != MillenniumDosGxStartupSessionState::post_overlay_private_interrupt_boundary) {
            result.error = "Millennium DOS GX startup trace did not reach its documented boundary";
            return result;
        }
        result.session.emplace(std::move(session));
    } catch (const std::exception& exception) {
        result.error = std::string("Millennium DOS GX startup trace admission rejected: ") + exception.what();
    }
    return result;
}

} // namespace eon
