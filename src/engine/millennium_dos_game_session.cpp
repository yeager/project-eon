#include "engine/millennium_dos_game_session.hpp"

#include <stdexcept>

namespace eon {

MillenniumDosGameSession::MillenniumDosGameSession(MillenniumDosGameFlow flow)
    : flow_(flow) {
    if (flow_.action_poll_address != 0x10f05 || flow_.function_key_count != 10
        || flow_.function_key_table_stride != 8 || flow_.function_key_dispatch_address != 0x76f0) {
        throw std::runtime_error("Unsupported Millennium DOS action-dispatch profile");
    }
}

std::optional<std::size_t> MillenniumDosGameSession::observe_action(const std::uint8_t action) {
    last_function_key_index_.reset();
    last_special_action_.reset();
    if (action == flow_.special_action_0 || action == flow_.special_action_1) {
        last_special_action_ = action;
        return std::nullopt;
    }
    const auto normalized = static_cast<unsigned>(action) - flow_.function_key_first_action;
    if (action < flow_.function_key_first_action || normalized >= flow_.function_key_count) {
        return std::nullopt;
    }
    last_function_key_index_ = normalized;
    return last_function_key_index_;
}

} // namespace eon
