#include "engine/millennium_dos_game_session.hpp"

#include <stdexcept>
#include <utility>

namespace eon {

MillenniumDosGameSession::MillenniumDosGameSession(MillenniumDosGameFlow flow)
    : flow_(flow) {
    if (flow_.action_poll_address != 0x0f05 || flow_.function_key_count != 10
        || flow_.function_key_table_stride != 8 || flow_.function_key_dispatch_address != 0x76f0
        || flow_.eighth_function_key.handler_address != 0x7306
        || flow_.eighth_function_key.reset_runtime_byte_address != 0xda30
        || flow_.eighth_function_key.reset_runtime_byte_value != 0) {
        throw std::runtime_error("Unsupported Millennium DOS action-dispatch profile");
    }
}

MillenniumDosGameSession::MillenniumDosGameSession(MillenniumDosGameFlow flow,
    const std::span<const std::uint8_t> game_executable)
    : MillenniumDosGameSession(std::move(flow)) {
    if (game_executable.empty()) {
        throw std::runtime_error("Millennium DOS special-action observation needs original executable");
    }
    game_executable_ = game_executable;
}

void MillenniumDosGameSession::clear_last_observation() {
    last_function_key_index_.reset();
    last_special_action_.reset();
    last_first_special_action_trace_.reset();
    last_second_special_action_trace_.reset();
    last_special_runtime_byte_effect_.reset();
    last_first_function_key_trace_.reset();
    last_second_function_key_trace_.reset();
    last_third_function_key_trace_.reset();
    last_fourth_function_key_trace_.reset();
    last_fifth_function_key_trace_.reset();
    last_sixth_function_key_trace_.reset();
    last_seventh_function_key_trace_.reset();
    last_eighth_function_key_trace_.reset();
    last_ninth_function_key_trace_.reset();
    last_tenth_function_key_trace_.reset();
    last_runtime_byte_effect_.reset();
}

MillenniumDosFirstSpecialActionPrefix MillenniumDosGameSession::observe_first_special_action(
    const MillenniumDosRuntimeByteObservation observation) {
    if (game_executable_.empty()) {
        throw std::runtime_error("Millennium DOS special-action observation needs original executable");
    }
    if (observation.address != 0x07f9) {
        throw std::runtime_error("Millennium DOS first special-action observation has an unsupported runtime address");
    }
    clear_last_observation();
    const auto trace = evaluate_millennium_dos_first_special_action_prefix(
        game_executable_, observation.value);
    if (trace.action != flow_.special_action_0 || trace.runtime_byte_address != observation.address) {
        throw std::runtime_error("Unsupported Millennium DOS first special-action profile");
    }
    last_special_action_ = trace.action;
    last_special_runtime_byte_effect_ = MillenniumDosRuntimeByteEffect{
        .address = trace.runtime_byte_address,
        // This is the explicitly supplied native observation. Do not reuse a
        // previous host reconstruction as the actual pre-write value.
        .previous = observation.value,
        .value = trace.toggled_runtime_byte,
    };
    reconstructed_07f9_ = trace.toggled_runtime_byte;
    last_first_special_action_trace_ = trace;
    return trace;
}

MillenniumDosSecondSpecialActionPrefix MillenniumDosGameSession::observe_second_special_action(
    const MillenniumDosRuntimeByteObservation observation) {
    if (game_executable_.empty()) {
        throw std::runtime_error("Millennium DOS special-action observation needs original executable");
    }
    if (observation.address != 0xda3a) {
        throw std::runtime_error("Millennium DOS second special-action observation has an unsupported runtime address");
    }
    clear_last_observation();
    const auto trace = evaluate_millennium_dos_second_special_action_prefix(
        game_executable_, observation.value);
    if (trace.action != flow_.special_action_1 || trace.runtime_byte_address != observation.address) {
        throw std::runtime_error("Unsupported Millennium DOS second special-action profile");
    }
    last_special_action_ = trace.action;
    last_second_special_action_trace_ = trace;
    return trace;
}

std::optional<std::size_t> MillenniumDosGameSession::observe_action(
    const MillenniumDosActionObservation observation) {
    if (observation.poll_address != flow_.action_poll_address) {
        throw std::runtime_error("Millennium DOS action observation is detached from the verified poll site");
    }
    clear_last_observation();
    const auto action = observation.action;
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
        // Exact effect of the verified F8 prefix:
        //   0e 1f c6 06 30 da 00 b0 02 ...
        // `mov byte ptr [$da30], 0` executes before any conditional branch
        // or external call. Preserve the unknown initial value rather than
        // fabricating one from unrelated serialized save data.
        last_runtime_byte_effect_ = MillenniumDosRuntimeByteEffect{
            .address = flow_.eighth_function_key.reset_runtime_byte_address,
            .previous = reconstructed_da30_,
            .value = flow_.eighth_function_key.reset_runtime_byte_value,
        };
        reconstructed_da30_ = flow_.eighth_function_key.reset_runtime_byte_value;
    } else if (normalized == 8) {
        last_ninth_function_key_trace_ = flow_.ninth_function_key;
    } else if (normalized == 9) {
        last_tenth_function_key_trace_ = flow_.tenth_function_key;
    }
    return last_function_key_index_;
}

std::optional<std::uint8_t> MillenniumDosGameSession::reconstructed_runtime_byte(
    const std::uint16_t address) const {
    if (address == flow_.eighth_function_key.reset_runtime_byte_address) return reconstructed_da30_;
    if (address == 0x07f9) return reconstructed_07f9_;
    return std::nullopt;
}

} // namespace eon
