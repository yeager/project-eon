#include "engine/millennium_dos_eighth_function_session.hpp"

#include <stdexcept>

namespace eon {

MillenniumDosEighthFunctionSession::MillenniumDosEighthFunctionSession(
    const std::span<const std::uint8_t> executable)
    : trace_(parse_millennium_dos_game_flow(executable).eighth_function_key) {
    if (trace_.handler_address != 0x7306 || trace_.reset_runtime_byte_address != 0xda30
        || trace_.reset_runtime_byte_value != 0 || trace_.initial_al_value != 2
        || trace_.local_preflight_address != 0x731a
        || trace_.repeated_call_address != 0x09fa || trace_.repeat_shift_register != 3) {
        throw std::runtime_error("Unsupported Millennium DOS eighth-function profile");
    }
    effects_.push_back({0x7308, trace_.reset_runtime_byte_address,
        trace_.reset_runtime_byte_value});
}

MillenniumDosEighthFunctionBoundary MillenniumDosEighthFunctionSession::boundary() const {
    switch (state_) {
    case MillenniumDosEighthFunctionState::awaiting_preflight_call_return:
        return {MillenniumDosEighthFunctionBoundaryKind::call_return, 0x730f,
            trace_.local_preflight_address, trace_.initial_al_value, 0};
    case MillenniumDosEighthFunctionState::awaiting_wait_call_return:
        return {MillenniumDosEighthFunctionBoundaryKind::call_return, 0x7312,
            trace_.repeated_call_address, std::nullopt, wait_iteration_};
    case MillenniumDosEighthFunctionState::awaiting_wait_bl:
        return {MillenniumDosEighthFunctionBoundaryKind::register_bl, 0x7315,
            std::nullopt, std::nullopt, wait_iteration_};
    case MillenniumDosEighthFunctionState::returned:
        return {MillenniumDosEighthFunctionBoundaryKind::local_return, 0x7319,
            std::nullopt, std::nullopt, wait_iteration_};
    }
    throw std::runtime_error("Unsupported Millennium DOS eighth-function state");
}

void MillenniumDosEighthFunctionSession::observe_call_return(
    const std::uint16_t call_address, const std::uint16_t return_address) {
    const auto expected = boundary();
    if (expected.kind != MillenniumDosEighthFunctionBoundaryKind::call_return
        || expected.instruction_address != call_address
        || return_address != static_cast<std::uint16_t>(call_address + 3U)) {
        throw std::runtime_error("Millennium DOS eighth-function return is detached from its call");
    }
    state_ = state_ == MillenniumDosEighthFunctionState::awaiting_preflight_call_return
        ? MillenniumDosEighthFunctionState::awaiting_wait_call_return
        : MillenniumDosEighthFunctionState::awaiting_wait_bl;
}

void MillenniumDosEighthFunctionSession::observe_bl(
    const std::uint16_t shift_address, const std::uint8_t value) {
    if (state_ != MillenniumDosEighthFunctionState::awaiting_wait_bl
        || shift_address != 0x7315) {
        throw std::runtime_error("Millennium DOS eighth-function BL observation is detached");
    }
    shifted_bl_values_.push_back(static_cast<std::uint8_t>(value >> 1U));
    if ((value & 1U) != 0) {
        ++wait_iteration_;
        state_ = MillenniumDosEighthFunctionState::awaiting_wait_call_return;
    } else {
        state_ = MillenniumDosEighthFunctionState::returned;
    }
}

} // namespace eon
