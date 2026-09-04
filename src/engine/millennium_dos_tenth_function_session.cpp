#include "engine/millennium_dos_tenth_function_session.hpp"

#include <stdexcept>

namespace eon {
namespace {
constexpr std::uint16_t handler_entry = 0x7384;
constexpr std::uint16_t handler_return = 0x740e;
}

MillenniumDosTenthFunctionSession::MillenniumDosTenthFunctionSession(
    const std::span<const std::uint8_t> game_executable)
    : game_executable_(game_executable),
      trace_(parse_millennium_dos_game_flow(game_executable).tenth_function_key) {
    if (trace_.handler_address != handler_entry
        || trace_.initialization_guard_address != 0xa19e
        || trace_.display_selector_call_address != 0xd0c9
        || trace_.enabled_call_address != 0x7b47
        || trace_.local_preflight_address != 0x731a
        || trace_.conditional_call_address != 0x7a9d
        || trace_.first_terminal_call_address != 0x4140
        || trace_.second_terminal_call_address != 0x7bcb
        || trace_.third_terminal_call_address != 0xa2a0
        || trace_.wait_call_address != 0x09fa
        || trace_.final_call_address != 0x4111) {
        throw std::runtime_error("Unsupported Millennium DOS tenth-function profile");
    }
}

MillenniumDosTenthFunctionBoundary MillenniumDosTenthFunctionSession::boundary() const {
    switch (state_) {
    case MillenniumDosTenthFunctionState::awaiting_initialization_guard:
        return {MillenniumDosTenthFunctionBoundaryKind::runtime_word,
            handler_entry, trace_.initialization_guard_address, std::nullopt, 0};
    case MillenniumDosTenthFunctionState::awaiting_enabled_byte:
        return {MillenniumDosTenthFunctionBoundaryKind::runtime_byte,
            0x73a3, trace_.enabled_runtime_byte_address, std::nullopt, 0};
    case MillenniumDosTenthFunctionState::awaiting_limit_byte:
        return {MillenniumDosTenthFunctionBoundaryKind::runtime_byte,
            0x73ad, trace_.limit_runtime_byte_address, std::nullopt, limit_loop_count_};
    case MillenniumDosTenthFunctionState::awaiting_conditional_byte:
        return {MillenniumDosTenthFunctionBoundaryKind::runtime_byte,
            0x73bf, trace_.conditional_runtime_byte_address, std::nullopt, 0};
    case MillenniumDosTenthFunctionState::awaiting_wait_byte:
        return {MillenniumDosTenthFunctionBoundaryKind::runtime_byte,
            0x73d2, trace_.wait_runtime_byte_address, std::nullopt, wait_loop_count_};
    case MillenniumDosTenthFunctionState::awaiting_busy_guard:
        return {MillenniumDosTenthFunctionBoundaryKind::runtime_word,
            0x73e7, trace_.initialization_guard_address, std::nullopt, 1};
    case MillenniumDosTenthFunctionState::awaiting_busy_word:
        return {MillenniumDosTenthFunctionBoundaryKind::runtime_word,
            0x73f7, std::uint16_t{0x07da}, std::nullopt, 0};
    case MillenniumDosTenthFunctionState::awaiting_wait_zero_flag:
        return {MillenniumDosTenthFunctionBoundaryKind::zero_flag,
            0x73dd, std::nullopt, std::nullopt, wait_loop_count_};
    case MillenniumDosTenthFunctionState::awaiting_wait_bl:
        return {MillenniumDosTenthFunctionBoundaryKind::register_bl,
            0x73df, std::nullopt, std::nullopt, wait_loop_count_};
    case MillenniumDosTenthFunctionState::returned_by_guard:
        return {MillenniumDosTenthFunctionBoundaryKind::local_return,
            0x738b, std::nullopt, std::nullopt, 0};
    case MillenniumDosTenthFunctionState::returned:
        return {MillenniumDosTenthFunctionBoundaryKind::local_return,
            handler_return, std::nullopt, std::nullopt, 0};
    default:
        return {MillenniumDosTenthFunctionBoundaryKind::call_return,
            call_address_, std::nullopt, call_target_, 0};
    }
}

void MillenniumDosTenthFunctionSession::enter_call(
    const MillenniumDosTenthFunctionState state, const std::uint16_t address,
    const std::uint32_t target) {
    state_ = state;
    call_address_ = address;
    call_target_ = target;
}

void MillenniumDosTenthFunctionSession::record_write(
    const std::uint16_t address, const std::uint8_t value) {
    std::optional<std::uint8_t> previous;
    if (address == trace_.local_mode_address) previous = local_mode_;
    runtime_effects_.push_back({address, previous, value});
    if (address == trace_.local_mode_address) local_mode_ = value;
}

void MillenniumDosTenthFunctionSession::observe_runtime_word(
    const std::uint16_t instruction_address, const std::uint16_t runtime_address,
    const std::uint16_t value) {
    const auto expected = boundary();
    if (expected.kind != MillenniumDosTenthFunctionBoundaryKind::runtime_word
        || expected.instruction_address != instruction_address
        || expected.runtime_address != runtime_address) {
        throw std::runtime_error("Millennium DOS tenth-function word observation is detached");
    }
    if (state_ == MillenniumDosTenthFunctionState::awaiting_initialization_guard) {
        if (value != 0) {
            state_ = MillenniumDosTenthFunctionState::returned_by_guard;
            return;
        }
        enter_call(MillenniumDosTenthFunctionState::awaiting_display_call_return,
            0x738e, trace_.display_selector_call_address);
        return;
    }
    if (state_ == MillenniumDosTenthFunctionState::awaiting_busy_guard) {
        if (value == 0) {
            enter_busy_chain(0x73ee, trace_.final_call_address,
                MillenniumDosTenthFunctionState::awaiting_busy_optional_final_call_return);
        } else {
            enter_busy_chain(0x73f1, 0xbe28,
                MillenniumDosTenthFunctionState::awaiting_busy_call_return);
        }
        return;
    }
    if (state_ == MillenniumDosTenthFunctionState::awaiting_busy_word) {
        enter_busy_chain(0x73fe, 0x0ae3,
            MillenniumDosTenthFunctionState::awaiting_busy_dx_call_return);
        return;
    }
    throw std::runtime_error("Unsupported Millennium DOS tenth-function word state");
}

void MillenniumDosTenthFunctionSession::observe_runtime_byte(
    const std::uint16_t instruction_address, const std::uint16_t runtime_address,
    const std::uint8_t value) {
    const auto expected = boundary();
    if (expected.kind != MillenniumDosTenthFunctionBoundaryKind::runtime_byte
        || expected.instruction_address != instruction_address
        || expected.runtime_address != runtime_address) {
        throw std::runtime_error("Millennium DOS tenth-function byte observation is detached");
    }
    if (state_ == MillenniumDosTenthFunctionState::awaiting_enabled_byte) {
        if (value != 0) {
            enter_call(MillenniumDosTenthFunctionState::awaiting_enabled_call_return,
                0x73aa, trace_.enabled_call_address);
        } else {
            state_ = MillenniumDosTenthFunctionState::awaiting_limit_byte;
        }
        return;
    }
    if (state_ == MillenniumDosTenthFunctionState::awaiting_limit_byte) {
        if (value >= trace_.limit_value) {
            ++limit_loop_count_;
            enter_call(MillenniumDosTenthFunctionState::awaiting_preflight_call_return,
                0x73b4, trace_.local_preflight_address);
        } else {
            record_write(trace_.local_mode_address, trace_.local_mode_reset_value);
            state_ = MillenniumDosTenthFunctionState::awaiting_conditional_byte;
        }
        return;
    }
    if (state_ == MillenniumDosTenthFunctionState::awaiting_conditional_byte) {
        if (value == 0) {
            enter_call(MillenniumDosTenthFunctionState::awaiting_conditional_call_return,
                0x73c6, trace_.conditional_call_address);
        } else {
            enter_terminal_chain(0x73c9, trace_.first_terminal_call_address);
        }
        return;
    }
    if (state_ == MillenniumDosTenthFunctionState::awaiting_wait_byte) {
        if (value != 0) {
            state_ = MillenniumDosTenthFunctionState::awaiting_busy_guard;
        } else {
            enter_call(MillenniumDosTenthFunctionState::awaiting_wait_call_return,
                0x73da, trace_.wait_call_address);
        }
        return;
    }
    throw std::runtime_error("Unsupported Millennium DOS tenth-function byte state");
}

void MillenniumDosTenthFunctionSession::observe_call_return(
    const std::uint16_t call_address, const std::uint16_t return_address) {
    const auto expected = boundary();
    if (expected.kind != MillenniumDosTenthFunctionBoundaryKind::call_return
        || expected.instruction_address != call_address
        || return_address != static_cast<std::uint16_t>(call_address + 3U)) {
        throw std::runtime_error("Millennium DOS tenth-function return is detached from its call");
    }
    switch (state_) {
    case MillenniumDosTenthFunctionState::awaiting_display_call_return:
        record_write(trace_.first_reset_runtime_byte_address,
            trace_.first_reset_runtime_byte_value);
        record_write(trace_.second_reset_runtime_byte_address,
            trace_.second_reset_runtime_byte_value);
        record_write(trace_.local_mode_address, trace_.local_mode_value);
        state_ = MillenniumDosTenthFunctionState::awaiting_enabled_byte;
        return;
    case MillenniumDosTenthFunctionState::awaiting_enabled_call_return:
    case MillenniumDosTenthFunctionState::awaiting_preflight_call_return:
        state_ = MillenniumDosTenthFunctionState::awaiting_limit_byte;
        return;
    case MillenniumDosTenthFunctionState::awaiting_conditional_call_return:
        enter_terminal_chain(0x73c9, trace_.first_terminal_call_address);
        return;
    case MillenniumDosTenthFunctionState::awaiting_terminal_call_return:
        if (call_address == 0x73c9) {
            enter_terminal_chain(0x73cc, trace_.second_terminal_call_address);
        } else if (call_address == 0x73cc) {
            enter_terminal_chain(0x73cf, trace_.third_terminal_call_address);
        } else {
            state_ = MillenniumDosTenthFunctionState::awaiting_wait_byte;
        }
        return;
    case MillenniumDosTenthFunctionState::awaiting_wait_call_return:
        state_ = MillenniumDosTenthFunctionState::awaiting_wait_zero_flag;
        return;
    case MillenniumDosTenthFunctionState::awaiting_final_call_return:
        state_ = MillenniumDosTenthFunctionState::returned;
        return;
    case MillenniumDosTenthFunctionState::awaiting_busy_optional_final_call_return:
        enter_busy_chain(0x73f1, 0xbe28,
            MillenniumDosTenthFunctionState::awaiting_busy_call_return);
        return;
    case MillenniumDosTenthFunctionState::awaiting_busy_call_return:
        if (call_address == 0x73f1) {
            enter_busy_chain(0x73f4, 0x0b9d,
                MillenniumDosTenthFunctionState::awaiting_busy_call_return);
        } else {
            state_ = MillenniumDosTenthFunctionState::awaiting_busy_word;
        }
        return;
    case MillenniumDosTenthFunctionState::awaiting_busy_dx_call_return:
        enter_busy_chain(0x7401, 0x4bf7,
            MillenniumDosTenthFunctionState::awaiting_busy_last_call_return);
        return;
    case MillenniumDosTenthFunctionState::awaiting_busy_last_call_return:
        record_write(trace_.wait_runtime_byte_address, 0);
        record_write(0xda42, 0x80);
        state_ = MillenniumDosTenthFunctionState::returned;
        return;
    default:
        throw std::runtime_error("Unsupported Millennium DOS tenth-function call-return state");
    }
}

void MillenniumDosTenthFunctionSession::observe_zero_flag(
    const std::uint16_t branch_address, const bool set) {
    if (state_ != MillenniumDosTenthFunctionState::awaiting_wait_zero_flag
        || branch_address != 0x73dd) {
        throw std::runtime_error("Millennium DOS tenth-function ZF observation is detached");
    }
    if (set) {
        ++wait_loop_count_;
        enter_terminal_chain(0x73cc, trace_.second_terminal_call_address);
    } else {
        state_ = MillenniumDosTenthFunctionState::awaiting_wait_bl;
    }
}

void MillenniumDosTenthFunctionSession::observe_bl(
    const std::uint16_t shift_address, const std::uint8_t value) {
    if (state_ != MillenniumDosTenthFunctionState::awaiting_wait_bl
        || shift_address != 0x73df) {
        throw std::runtime_error("Millennium DOS tenth-function BL observation is detached");
    }
    if ((value & 1U) != 0) {
        ++wait_loop_count_;
        enter_terminal_chain(0x73cc, trace_.second_terminal_call_address);
    } else {
        enter_call(MillenniumDosTenthFunctionState::awaiting_final_call_return,
            0x73e3, trace_.final_call_address);
    }
}

void MillenniumDosTenthFunctionSession::enter_terminal_chain(
    const std::uint16_t address, const std::uint32_t target) {
    enter_call(MillenniumDosTenthFunctionState::awaiting_terminal_call_return,
        address, target);
}

void MillenniumDosTenthFunctionSession::enter_busy_chain(
    const std::uint16_t address, const std::uint32_t target,
    const MillenniumDosTenthFunctionState state) {
    enter_call(state, address, target);
}

} // namespace eon
