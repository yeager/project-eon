#include "engine/millennium_dos_function_dispatch_admission.hpp"

#include "data/millennium_dos_game_flow.hpp"

#include <array>
#include <exception>
#include <optional>

namespace eon {

MillenniumDosFunctionDispatchAdmission admit_millennium_dos_function_dispatch(
    const std::span<const std::uint8_t> game_executable,
    const MillenniumDosPostOverlayLoopSession& loop,
    const MillenniumDosFunctionDispatchObservation observation) {
    MillenniumDosFunctionDispatchAdmission result;
    try {
        const auto flow = parse_millennium_dos_game_flow(game_executable);
        const std::array<std::uint16_t, 10> handlers{{
            flow.first_function_key.handler_address,
            flow.second_function_key.handler_address,
            flow.third_function_key.handler_address,
            flow.fourth_function_key.handler_address,
            flow.fifth_function_key.handler_address,
            flow.sixth_function_key.handler_address,
            flow.seventh_function_key.handler_address,
            flow.eighth_function_key.handler_address,
            flow.ninth_function_key.handler_address,
            flow.tenth_function_key.handler_address,
        }};
        const auto boundary = loop.boundary();
        const auto loop_index = loop.function_key_index();
        if (loop.state() != MillenniumDosPostOverlayLoopState::dispatch_call_boundary
            || boundary.kind != MillenniumDosPostOverlayLoopBoundaryKind::dispatch_call
            || boundary.instruction_address != 0xd40a
            || boundary.call_target != std::optional<std::uint16_t>{0x76f1}
            || !loop_index || *loop_index >= handlers.size()
            || observation.scaled_call_address != boundary.instruction_address
            || observation.dispatcher_address != *boundary.call_target
            || observation.function_key_index != *loop_index
            || observation.handler_address != handlers[*loop_index]) {
            result.error = "Function handler observation is detached from exact scaled dispatch";
            return result;
        }
        result.accepted = true;
        result.function_key_index = *loop_index;
        result.handler_address = handlers[*loop_index];
    } catch (const std::exception& exception) {
        result.error = std::string("Function dispatch admission rejected: ") + exception.what();
    }
    return result;
}

} // namespace eon
