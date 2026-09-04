#include "engine/millennium_dos_first_function_session.hpp"

#include <stdexcept>

namespace eon {

MillenniumDosFirstFunctionSession::MillenniumDosFirstFunctionSession(
    const std::span<const std::uint8_t> executable)
    : trace_(parse_millennium_dos_game_flow(executable).first_function_key) {
    if (trace_.handler_address != 0x6f9a
        || trace_.display_selector_call_address != 0xd0c9
        || trace_.setup_entry_address != 0x771d || trace_.selector_address != 0xda1f
        || trace_.selected_record_address != 0x12cc
        || trace_.selected_record_storage_address != 0xda20
        || trace_.screen_descriptor_address != 0x300f
        || trace_.screen_selector_storage_address != 0x75a8
        || trace_.screen_descriptor_storage_address != 0x75a6
        || trace_.setup_first_call_address != 0x5b1f
        || trace_.selected_record_byte_2 != 0x11 || trace_.selected_record_byte_36 != 0) {
        throw std::runtime_error("Unsupported Millennium DOS first-function profile");
    }
}

MillenniumDosFirstFunctionBoundary MillenniumDosFirstFunctionSession::boundary() const {
    switch (state_) {
    case MillenniumDosFirstFunctionState::awaiting_display_return:
        return {MillenniumDosFirstFunctionBoundaryKind::call_return, 0x6f9c,
            trace_.display_selector_call_address, std::uint16_t{0}, 0};
    case MillenniumDosFirstFunctionState::awaiting_setup_first_return:
        return {MillenniumDosFirstFunctionBoundaryKind::call_return, 0x774d,
            trace_.setup_first_call_address, std::uint16_t{1}, 0};
    case MillenniumDosFirstFunctionState::awaiting_setup_second_return:
        return {MillenniumDosFirstFunctionBoundaryKind::call_return, 0x7750,
            std::uint16_t{0x7d60}, std::nullopt, 0};
    case MillenniumDosFirstFunctionState::awaiting_terminal_return:
        return {MillenniumDosFirstFunctionBoundaryKind::call_return, 0x777f,
            std::uint16_t{0x0b0c}, std::nullopt, 0};
    case MillenniumDosFirstFunctionState::awaiting_wait_return:
        return {MillenniumDosFirstFunctionBoundaryKind::call_return, 0x7782,
            std::uint16_t{0x09fa}, std::nullopt, wait_iteration_};
    case MillenniumDosFirstFunctionState::awaiting_wait_bl:
        return {MillenniumDosFirstFunctionBoundaryKind::register_bl, 0x7785,
            std::nullopt, std::nullopt, wait_iteration_};
    case MillenniumDosFirstFunctionState::returned:
        return {MillenniumDosFirstFunctionBoundaryKind::local_return, 0x7789,
            std::nullopt, std::nullopt, 0};
    }
    throw std::runtime_error("Unsupported Millennium DOS first-function state");
}

void MillenniumDosFirstFunctionSession::enter_setup() {
    effects_.push_back({0x7720, trace_.selector_address, 1, trace_.selector_value});
    effects_.push_back({0x7725, trace_.selected_record_storage_address, 2,
        trace_.selected_record_address});
    effects_.push_back({0x7739, trace_.screen_selector_storage_address, 1,
        trace_.screen_descriptor_mode});
    effects_.push_back({0x773c, trace_.screen_descriptor_storage_address, 2,
        trace_.screen_descriptor_address});
    state_ = MillenniumDosFirstFunctionState::awaiting_setup_first_return;
}

void MillenniumDosFirstFunctionSession::observe_call_return(
    const std::uint16_t call, const std::uint16_t returned_to) {
    const auto expected = boundary();
    if (expected.kind != MillenniumDosFirstFunctionBoundaryKind::call_return
        || expected.instruction_address != call
        || returned_to != static_cast<std::uint16_t>(call + 3U)) {
        throw std::runtime_error("Detached Millennium DOS first-function return");
    }
    switch (state_) {
    case MillenniumDosFirstFunctionState::awaiting_display_return:
        enter_setup(); return;
    case MillenniumDosFirstFunctionState::awaiting_setup_first_return:
        state_ = MillenniumDosFirstFunctionState::awaiting_setup_second_return; return;
    case MillenniumDosFirstFunctionState::awaiting_setup_second_return:
        effects_.push_back({0x7753, 0xda09, 1, 0});
        effects_.push_back({0x775f, 0xda39, 1, trace_.selected_record_byte_2});
        // Byte 36 is hash-verified zero, so the conditional calls are not reached.
        effects_.push_back({0x777a, 0xda13, 1, 0});
        state_ = MillenniumDosFirstFunctionState::awaiting_terminal_return; return;
    case MillenniumDosFirstFunctionState::awaiting_terminal_return:
        state_ = MillenniumDosFirstFunctionState::awaiting_wait_return; return;
    case MillenniumDosFirstFunctionState::awaiting_wait_return:
        state_ = MillenniumDosFirstFunctionState::awaiting_wait_bl; return;
    default: throw std::runtime_error("Unsupported first-function call state");
    }
}

void MillenniumDosFirstFunctionSession::observe_bl(
    const std::uint16_t instruction, const std::uint8_t value) {
    if (state_ != MillenniumDosFirstFunctionState::awaiting_wait_bl
        || instruction != 0x7785) {
        throw std::runtime_error("Detached Millennium DOS first-function BL");
    }
    if ((value & 1U) != 0) {
        ++wait_iteration_;
        state_ = MillenniumDosFirstFunctionState::awaiting_wait_return;
    } else state_ = MillenniumDosFirstFunctionState::returned;
}

} // namespace eon
