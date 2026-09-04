#include "engine/millennium_dos_third_function_session.hpp"

#include <stdexcept>

namespace eon {

MillenniumDosThirdFunctionSession::MillenniumDosThirdFunctionSession(
    const std::span<const std::uint8_t> executable)
    : trace_(parse_millennium_dos_game_flow(executable).third_function_key) {
    if (trace_.handler_address != 0x6faa
        || trace_.initialization_guard_address != 0xa19e
        || trace_.availability_address != 0xda27
        || trace_.wait_call_address != 0x09fa
        || trace_.callback_slot_address != 0x6f98
        || trace_.callback_address != 0x712a
        || trace_.list_mode_address != 0x6e98
        || trace_.list_mode_value != 0
        || trace_.source_far_pointer_address != 0x0112
        || trace_.list_address != 0x6e99) {
        throw std::runtime_error("Unsupported Millennium DOS third-function profile");
    }
}

MillenniumDosThirdFunctionBoundary MillenniumDosThirdFunctionSession::boundary() const {
    switch (state_) {
    case MillenniumDosThirdFunctionState::awaiting_initialization_guard:
        return {MillenniumDosThirdFunctionBoundaryKind::runtime_word, 0x6faa,
            trace_.initialization_guard_address, std::nullopt, std::nullopt, 0};
    case MillenniumDosThirdFunctionState::awaiting_availability_word:
        return {MillenniumDosThirdFunctionBoundaryKind::runtime_word, 0x6fb4,
            trace_.availability_address, std::nullopt, std::nullopt, 0};
    case MillenniumDosThirdFunctionState::awaiting_wait_call_return:
        return {MillenniumDosThirdFunctionBoundaryKind::call_return, 0x6fbe,
            std::nullopt, trace_.wait_call_address, std::nullopt, wait_iteration_};
    case MillenniumDosThirdFunctionState::awaiting_wait_bl:
        return {MillenniumDosThirdFunctionBoundaryKind::register_bl, 0x6fc1,
            std::nullopt, std::nullopt, std::nullopt, wait_iteration_};
    case MillenniumDosThirdFunctionState::awaiting_first_setup_call_return:
        return {MillenniumDosThirdFunctionBoundaryKind::call_return, 0x6fd4,
            std::nullopt, std::uint16_t{0x4d2c}, std::uint16_t{0x16}, 0};
    case MillenniumDosThirdFunctionState::awaiting_second_setup_call_return:
        return {MillenniumDosThirdFunctionBoundaryKind::call_return, 0x6fda,
            std::nullopt, std::uint16_t{0x4d36}, std::uint16_t{0x17}, 0};
    case MillenniumDosThirdFunctionState::awaiting_setup_count_word:
        return {MillenniumDosThirdFunctionBoundaryKind::runtime_word, 0x6fe3,
            trace_.availability_address, std::nullopt, std::nullopt, 0};
    case MillenniumDosThirdFunctionState::source_far_pointer_boundary:
        return {MillenniumDosThirdFunctionBoundaryKind::far_pointer, 0x6fee,
            trace_.source_far_pointer_address, std::nullopt, std::nullopt, 0};
    case MillenniumDosThirdFunctionState::returned_by_guard:
        return {MillenniumDosThirdFunctionBoundaryKind::local_return, 0x6fb1,
            std::nullopt, std::nullopt, std::nullopt, 0};
    case MillenniumDosThirdFunctionState::returned_by_wait:
        return {MillenniumDosThirdFunctionBoundaryKind::local_return, 0x6fc5,
            std::nullopt, std::nullopt, std::nullopt, 0};
    }
    throw std::runtime_error("Unsupported Millennium DOS third-function state");
}

void MillenniumDosThirdFunctionSession::observe_runtime_word(
    const std::uint16_t instruction, const std::uint16_t address,
    const std::uint16_t value) {
    const auto expected = boundary();
    if (expected.kind != MillenniumDosThirdFunctionBoundaryKind::runtime_word
        || expected.instruction_address != instruction
        || expected.runtime_address != address) {
        throw std::runtime_error("Detached Millennium DOS third-function word");
    }
    if (state_ == MillenniumDosThirdFunctionState::awaiting_initialization_guard) {
        state_ = value == 0 ? MillenniumDosThirdFunctionState::awaiting_availability_word
                            : MillenniumDosThirdFunctionState::returned_by_guard;
    } else if (state_ == MillenniumDosThirdFunctionState::awaiting_availability_word) {
        if ((value & 0xffU) == 0) {
            state_ = MillenniumDosThirdFunctionState::awaiting_wait_call_return;
        } else {
            effects_.push_back({0x6fc9, trace_.callback_slot_address, 2,
                trace_.callback_address});
            effects_.push_back({0x6fcc, trace_.list_mode_address, 1,
                trace_.list_mode_value});
            state_ = MillenniumDosThirdFunctionState::awaiting_first_setup_call_return;
        }
    } else {
        effects_.push_back({0x6fe7, 0x6e95, 1,
            static_cast<std::uint8_t>(value)});
        state_ = MillenniumDosThirdFunctionState::source_far_pointer_boundary;
    }
}

void MillenniumDosThirdFunctionSession::observe_call_return(
    const std::uint16_t call, const std::uint16_t returned_to) {
    const auto expected = boundary();
    if (expected.kind != MillenniumDosThirdFunctionBoundaryKind::call_return
        || expected.instruction_address != call
        || returned_to != static_cast<std::uint16_t>(call + 3U)) {
        throw std::runtime_error("Detached Millennium DOS third-function return");
    }
    if (state_ == MillenniumDosThirdFunctionState::awaiting_wait_call_return)
        state_ = MillenniumDosThirdFunctionState::awaiting_wait_bl;
    else if (state_ == MillenniumDosThirdFunctionState::awaiting_first_setup_call_return)
        state_ = MillenniumDosThirdFunctionState::awaiting_second_setup_call_return;
    else
        state_ = MillenniumDosThirdFunctionState::awaiting_setup_count_word;
}

void MillenniumDosThirdFunctionSession::observe_bl(
    const std::uint16_t instruction, const std::uint8_t value) {
    if (state_ != MillenniumDosThirdFunctionState::awaiting_wait_bl
        || instruction != 0x6fc1) {
        throw std::runtime_error("Detached Millennium DOS third-function BL");
    }
    if ((value & 1U) != 0) {
        ++wait_iteration_;
        state_ = MillenniumDosThirdFunctionState::awaiting_wait_call_return;
    } else {
        state_ = MillenniumDosThirdFunctionState::returned_by_wait;
    }
}

} // namespace eon
