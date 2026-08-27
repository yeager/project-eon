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
}

bool MillenniumDosTitleSession::poll_console(const bool character_available) {
    if (handed_off_ || !character_available) return false;
    handed_off_ = true;
    return true;
}

} // namespace eon
