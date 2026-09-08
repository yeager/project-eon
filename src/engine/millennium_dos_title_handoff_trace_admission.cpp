#include "engine/millennium_dos_title_handoff_trace_admission.hpp"

#include "data/millennium_dos_reference_trace.hpp"

#include <exception>

namespace eon {

MillenniumDosTitleHandoffTraceAdmission admit_millennium_dos_title_handoff_trace(
    const std::span<const std::uint8_t> mill_launcher,
    const std::span<const std::uint8_t> titles_executable, const std::string_view events) {
    MillenniumDosTitleHandoffTraceAdmission result;
    const auto observations = parse_millennium_dos_title_handoff_reference_observations(
        events, result.error);
    if (!observations) return result;
    try {
        MillenniumDosTitleToGameSession session(mill_launcher, titles_executable);
        session.observe_call_return(0x1c54, 0x1c57);
        session.observe_call_return(0x1c57, 0x1c5a);
        session.observe_stack_word(0x1c60, 0x1aa0, observations->restored_stack_pointer);
        session.observe_call_return(0x1c64, 0x1c67);
        session.observe_call_return(0x1a0f, 0x1a12);
        session.observe_title_termination(0x1a18, 0x4c00);
        session.observe_parent_exec_return(0x0337, false);
        session.observe_child_status(0x0348, 0, false);
        if (session.state() != MillenniumDosTitleToGameState::game_exec_boundary) {
            result.error = "Millennium DOS title-handoff trace did not reach the game EXEC boundary";
            return result;
        }
        result.session.emplace(std::move(session));
    } catch (const std::exception& exception) {
        result.error = std::string("Millennium DOS title-handoff trace admission rejected: ")
            + exception.what();
    }
    return result;
}

} // namespace eon
