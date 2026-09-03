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
    last_shared_helper_prefix_.reset();
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
    last_eighth_function_key_preflight_.reset();
    last_eighth_function_key_table_jump_.reset();
    last_eighth_function_key_selected_record_gate_.reset();
    last_eighth_function_key_runtime_effects_.clear();
}

MillenniumDosEighthFunctionKeyPreflight
MillenniumDosGameSession::observe_eighth_function_key_preflight(
    const MillenniumDosEighthFunctionKeyPreflightObservation observation) {
    if (game_executable_.empty()) {
        throw std::runtime_error("Millennium DOS F8 observation needs original executable");
    }
    if (observation.action.poll_address != flow_.action_poll_address
        || observation.action.action != flow_.function_key_first_action + 7U
        || observation.enabled_byte.address != flow_.eighth_function_key.preflight_runtime_byte_address
        || observation.counter_byte.address != flow_.eighth_function_key.decrement_runtime_byte_address) {
        throw std::runtime_error("Millennium DOS F8 observation is detached from the verified route");
    }
    // `observe_action` performs the byte-locked F8 table dispatch and records
    // its unconditional $da30 reset before this local preflight begins.
    if (observe_action(observation.action) != std::optional<std::size_t>{7}) {
        throw std::runtime_error("Unsupported Millennium DOS F8 action observation");
    }
    const auto trace = evaluate_millennium_dos_eighth_function_key_preflight(
        game_executable_, observation.enabled_byte.value, observation.counter_byte.value);
    if (trace.entry_address != flow_.eighth_function_key.local_preflight_address
        || trace.enabled_byte_address != observation.enabled_byte.address
        || trace.counter_byte_address != observation.counter_byte.address) {
        throw std::runtime_error("Unsupported Millennium DOS F8 preflight profile");
    }
    // DEC [$da0a] is the final deterministic write before XLAT. Record it
    // only on the branch where the validated evaluator proves it executes.
    if (trace.decremented_counter_byte) {
        last_eighth_function_key_runtime_effects_.push_back(MillenniumDosRuntimeByteEffect{
            .address = observation.counter_byte.address,
            .previous = observation.counter_byte.value,
            .value = *trace.decremented_counter_byte,
        });
        reconstructed_da0a_ = *trace.decremented_counter_byte;
    }
    last_eighth_function_key_preflight_ = trace;
    return trace;
}

MillenniumDosEighthFunctionKeyTableJumpPrefix
MillenniumDosGameSession::observe_eighth_function_key_table_jump(const std::uint8_t translated_al) {
    if (game_executable_.empty() || !last_eighth_function_key_preflight_
        || last_eighth_function_key_table_jump_
        || last_eighth_function_key_preflight_->outcome
            != MillenniumDosEighthFunctionKeyPreflightOutcome::table_jump_boundary) {
        throw std::runtime_error("Millennium DOS F8 table jump lacks an observed preflight boundary");
    }
    const auto trace = evaluate_millennium_dos_eighth_function_key_table_jump_prefix(
        game_executable_, translated_al);
    if (trace.entry_address != last_eighth_function_key_preflight_->table_jump_address) {
        throw std::runtime_error("Unsupported Millennium DOS F8 table-jump profile");
    }
    // Both MOV stores precede the next native gate at $6e2f. They are narrow
    // runtime overlays, not a selected game state or a writable save.
    last_eighth_function_key_runtime_effects_.push_back(MillenniumDosRuntimeByteEffect{
        .address = trace.reset_runtime_byte_address,
        .previous = reconstructed_da09_,
        .value = trace.reset_runtime_byte_value,
    });
    reconstructed_da09_ = trace.reset_runtime_byte_value;
    last_eighth_function_key_runtime_effects_.push_back(MillenniumDosRuntimeByteEffect{
        .address = trace.selected_runtime_byte_address,
        .previous = reconstructed_da06_,
        .value = trace.selected_runtime_byte_value,
    });
    reconstructed_da06_ = trace.selected_runtime_byte_value;
    last_eighth_function_key_table_jump_ = trace;
    return trace;
}

MillenniumDosEighthFunctionKeySelectedRecordGate
MillenniumDosGameSession::observe_eighth_function_key_selected_record_gate(
    const MillenniumDosRuntimeByteObservation observation) {
    if (game_executable_.empty() || !last_eighth_function_key_table_jump_
        || last_eighth_function_key_selected_record_gate_
        || observation.address != last_eighth_function_key_table_jump_->next_gate_runtime_byte_address) {
        throw std::runtime_error("Millennium DOS F8 selected-record gate lacks an observed table jump");
    }
    const auto trace = evaluate_millennium_dos_eighth_function_key_selected_record_gate(
        game_executable_, last_eighth_function_key_table_jump_->translated_al, observation.value);
    if (trace.selected_pointer != last_eighth_function_key_table_jump_->selected_pointer
        || trace.gate_runtime_byte_address != observation.address) {
        throw std::runtime_error("Unsupported Millennium DOS F8 selected-record gate profile");
    }
    last_eighth_function_key_selected_record_gate_ = trace;
    return trace;
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

MillenniumDosSharedHelperPrefix
MillenniumDosGameSession::observe_first_special_action_shared_helper_prefix() {
    if (game_executable_.empty() || !last_first_special_action_trace_ || last_shared_helper_prefix_) {
        throw std::runtime_error("Millennium DOS shared helper lacks an observed first special action");
    }
    const auto& first = *last_first_special_action_trace_;
    if (first.action != flow_.special_action_0 || first.helper_address != 0x0666) {
        throw std::runtime_error("Millennium DOS shared helper is detached from the verified action route");
    }
    const auto trace = evaluate_millennium_dos_shared_helper_prefix(
        game_executable_, first.selected_ax_value);
    if (trace.entry_address != first.helper_address || trace.caller_ax != first.selected_ax_value) {
        throw std::runtime_error("Unsupported Millennium DOS shared-helper profile");
    }
    last_shared_helper_prefix_ = trace;
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
    if (address == 0xda0a) return reconstructed_da0a_;
    if (address == 0xda09) return reconstructed_da09_;
    if (address == 0xda06) return reconstructed_da06_;
    return std::nullopt;
}

} // namespace eon
