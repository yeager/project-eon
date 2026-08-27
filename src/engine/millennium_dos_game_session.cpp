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
    last_first_function_key_trace_.reset();
    last_second_function_key_trace_.reset();
    last_third_function_key_trace_.reset();
    last_fourth_function_key_trace_.reset();
    last_fifth_function_key_trace_.reset();
    last_sixth_function_key_trace_.reset();
    last_seventh_function_key_trace_.reset();
    last_eighth_function_key_trace_.reset();
    last_ninth_function_key_trace_.reset();
    if (action == flow_.special_action_0 || action == flow_.special_action_1) {
        last_special_action_ = action;
        return std::nullopt;
    }
    const auto normalized = static_cast<unsigned>(action) - flow_.function_key_first_action;
    if (action < flow_.function_key_first_action || normalized >= flow_.function_key_count) {
        return std::nullopt;
    }
    last_function_key_index_ = normalized;
    if (normalized == 0) {
        last_first_function_key_trace_ = flow_.first_function_key;
    } else if (normalized == 1) {
        last_second_function_key_trace_ = flow_.second_function_key;
    } else if (normalized == 2) {
        last_third_function_key_trace_ = flow_.third_function_key;
    } else if (normalized == 3) {
        last_fourth_function_key_trace_ = flow_.fourth_function_key;
    } else if (normalized == 4) {
        last_fifth_function_key_trace_ = flow_.fifth_function_key;
    } else if (normalized == 5) {
        last_sixth_function_key_trace_ = flow_.sixth_function_key;
    } else if (normalized == 6) {
        last_seventh_function_key_trace_ = flow_.seventh_function_key;
    } else if (normalized == 7) {
        last_eighth_function_key_trace_ = flow_.eighth_function_key;
    } else if (normalized == 8) {
        last_ninth_function_key_trace_ = flow_.ninth_function_key;
    }
    return last_function_key_index_;
}

} // namespace eon
