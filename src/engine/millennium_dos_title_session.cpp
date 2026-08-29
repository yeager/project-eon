#include "engine/millennium_dos_title_session.hpp"

#include <stdexcept>
#include <utility>

namespace eon {

MillenniumDosTitleSession::MillenniumDosTitleSession(MillenniumDosTitleFlow flow)
    : flow_(std::move(flow)) {
    // The parser establishes these from code bytes.  Keep this guard here so
    // a caller cannot accidentally map a different DOS input convention onto
    // the title-to-game boundary.
    if (flow_.input_interrupt != 0x21 || flow_.input_service != 0x06
        || flow_.input_parameter != 0xff || flow_.exit_code != 0
        || flow_.launcher_title_program != "TITLES.EXE"
        || flow_.launcher_game_program != "2200ad.exe") {
        throw std::runtime_error("Unsupported Millennium DOS title hand-off profile");
    }
    input_boundary_ = {flow_.input_interrupt, flow_.input_service, flow_.input_parameter,
        flow_.input_nonzero_exit_address};
}

MillenniumDosTitleSession::MillenniumDosTitleSession(MillenniumDosSpanishTitleBoundary flow) {
    // Spanish title media is a distinct full hash identity.  Admit only its
    // independently parsed availability poll, not the English launcher or a
    // guessed child/driver return.
    if (flow.sha256 != "02082c35e18cee330f7d1b88098f502e68011f7e47a3a649961f6f03d1d14fe7"
        || flow.title_entry_address != 0x1b80 || flow.input_interrupt != 0x21
        || flow.input_service != 0x06 || flow.input_parameter != 0xff
        || flow.input_nonzero_exit_address != 0x1c54 || flow.private_wrapper_address != 0x0122
        || flow.post_title_entry_address != 0x1968 || flow.private_driver_function != 0x0013
        || flow.private_driver_call_count != 5 || flow.local_helper_address != 0x1917) {
        throw std::runtime_error("Unsupported Millennium Spanish DOS title hand-off profile");
    }
    input_boundary_ = {flow.input_interrupt, flow.input_service, flow.input_parameter,
        flow.input_nonzero_exit_address};
}

bool MillenniumDosTitleSession::poll_console(const bool character_available) {
    if (handed_off_ || !character_available) return false;
    handed_off_ = true;
    return true;
}

} // namespace eon
