#include "engine/millennium_dos_sibm_timer_session.hpp"

#include "data/sha256.hpp"

#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

namespace eon {
namespace {

constexpr std::string_view sibm_driver_sha256 =
    "f3224caa43c1149907f852fa98816ed68c489b70f1ba795592d684d4e51f31b1";
constexpr std::string_view timer_handler_sha256 =
    "3f2ab2b798a0ae1e2923de9d012581cd5a332946bf188f1bbd352107743221b1";
constexpr std::size_t timer_handler_offset = 0x034d;
constexpr std::size_t timer_handler_size = 0x0424 - timer_handler_offset;

std::uint16_t read_word(const std::span<const std::uint8_t> bytes,
    const std::size_t offset) {
    return static_cast<std::uint16_t>(bytes[offset]
        | (static_cast<std::uint16_t>(bytes[offset + 1U]) << 8U));
}

} // namespace

MillenniumDosSibmTimerSession::MillenniumDosSibmTimerSession(
    const std::span<const std::uint8_t> sibm_driver,
    const MillenniumDosSibmFunctionZeroCheckpoint& function_zero) {
    if (sibm_driver.size() != 2'871
        || to_hex(sha256(sibm_driver)) != sibm_driver_sha256
        || timer_handler_offset > sibm_driver.size()
        || timer_handler_size > sibm_driver.size() - timer_handler_offset
        || to_hex(sha256(sibm_driver.subspan(timer_handler_offset,
                timer_handler_size))) != timer_handler_sha256) {
        throw std::runtime_error("Unsupported Millennium SIBM timer-handler source");
    }
    const std::array<MillenniumDosSibmFunctionZeroMemoryEffect, 4> expected_effects{{
        {0x00cd, 1}, {0x00cf, 0}, {0x00d0, 0},
        {0x00b1, static_cast<std::uint8_t>(sibm_driver[0x00b1] | 0x10U)},
    }};
    if (function_zero.state
            != MillenniumDosSibmFunctionZeroState::awaiting_timer_vector_get
        || (function_zero.function_input != 0x0380
            && function_zero.function_input != 0x0220)
        || function_zero.dos_ax != 0x3508
        || function_zero.dos_interrupt_instruction != 0x0233
        || function_zero.next_instruction_address != 0x0233
        || function_zero.timer_handler_address != 0x034d
        || !function_zero.software_speaker_gate_disabled
        || function_zero.software_current_divisor != 0
        || function_zero.memory_effects.size() != expected_effects.size()) {
        throw std::invalid_argument(
            "SIBM timer session requires the exact function-zero AH=$35 boundary");
    }
    for (std::size_t index = 0; index < expected_effects.size(); ++index) {
        if (function_zero.memory_effects[index] != expected_effects[index]) {
            throw std::invalid_argument(
                "SIBM function-zero state does not match the original first-call effects");
        }
    }
    if ((sibm_driver[0x00b1] & 0x30U) != 0) {
        throw std::runtime_error("SIBM timer initial flags are not admitted");
    }

    checkpoint_.driver_flags = expected_effects.back().value;
    checkpoint_.software_current_divisor = 0;
    checkpoint_.countdown = read_word(sibm_driver, 0x00d2);
    checkpoint_.timer_accumulator = read_word(sibm_driver, 0x00c7);
    checkpoint_.timer_divisor = read_word(sibm_driver, 0x00c9);
    checkpoint_.sample_cursor_flag = sibm_driver[0x00cb];
    initial_driver_flags_ = sibm_driver[0x00b1];
    current_divisor_ = read_word(sibm_driver, 0x00cf);
}

void MillenniumDosSibmTimerSession::reject(std::string message) {
    checkpoint_.state = MillenniumDosSibmTimerState::rejected;
    checkpoint_.error = std::move(message);
}

MillenniumDosSibmTimerCheckpoint
MillenniumDosSibmTimerSession::activate_native_callback() {
    if (checkpoint_.state != MillenniumDosSibmTimerState::awaiting_native_activation
        || checkpoint_.driver_flags != static_cast<std::uint8_t>(initial_driver_flags_ | 0x10U)
        || current_divisor_ != 0 || checkpoint_.software_current_divisor != 0) {
        reject("SIBM native timer callback activation is one-shot or detached from function zero");
        return checkpoint_;
    }
    // This explicitly activates an Eon-owned callback. It does not imply that
    // DOS AH=$35 or AH=$25 ran, or that an original far vector was observed.
    checkpoint_.state = MillenniumDosSibmTimerState::running;
    return checkpoint_;
}

bool MillenniumDosSibmTimerSession::advance_handler_prefix() {
    // Exact conditional prefix from `$034d` through the first sample decision.
    // Function-zero leaves flags at `$10`, so the first startup tick follows
    // the inactive-sample path and reaches the shared timer tail.
    if ((checkpoint_.driver_flags & 0x08U) == 0) {
        checkpoint_.countdown = static_cast<std::uint16_t>(checkpoint_.countdown - 1U);
        if (checkpoint_.countdown == 0) {
            checkpoint_.countdown = 1;
            if ((checkpoint_.driver_flags & 0x01U) != 0) {
                checkpoint_.driver_flags = static_cast<std::uint8_t>(
                    (checkpoint_.driver_flags & 0xfeU) | 0x02U);
                checkpoint_.sample_cursor_flag = 1;
            } else if ((checkpoint_.driver_flags & 0x02U) != 0) {
                checkpoint_.state = MillenniumDosSibmTimerState::sample_dispatch_boundary;
                checkpoint_.next_instruction_address = checkpoint_.sample_cursor_flag != 0
                    ? 0x03ab : 0x03f3;
                if (checkpoint_.sample_cursor_flag == 0) {
                    checkpoint_.sample_cursor_flag = 1;
                }
                checkpoint_.last_action =
                    MillenniumDosSibmTimerAction::stop_before_sample_callback;
                return false;
            }
        }
    }
    return true;
}

void MillenniumDosSibmTimerSession::advance_timer_tail() {
    const auto sum = static_cast<std::uint32_t>(checkpoint_.timer_accumulator)
        + checkpoint_.timer_divisor;
    const bool carry = sum > 0xffffU;
    checkpoint_.timer_accumulator = static_cast<std::uint16_t>(sum);
    const bool previous_handler_due = carry || checkpoint_.timer_accumulator == 0xffffU;
    if (previous_handler_due) {
        checkpoint_.timer_accumulator = static_cast<std::uint16_t>(
            checkpoint_.timer_accumulator - 0xffffU);
        checkpoint_.last_action = MillenniumDosSibmTimerAction::request_host_clock_chain;
    } else {
        checkpoint_.last_action =
            MillenniumDosSibmTimerAction::completed_without_legacy_chain;
    }
    checkpoint_.next_instruction_address = 0x0424;
}

MillenniumDosSibmTimerCheckpoint MillenniumDosSibmTimerSession::advance_callback() {
    if (checkpoint_.state != MillenniumDosSibmTimerState::running) {
        reject("SIBM timer callback is not active or has reached an unsupported sample call");
        return checkpoint_;
    }
    if (checkpoint_.callback_count == std::numeric_limits<std::uint64_t>::max()) {
        reject("SIBM timer callback counter overflow");
        return checkpoint_;
    }
    if (!advance_handler_prefix()) return checkpoint_;
    advance_timer_tail();
    ++checkpoint_.callback_count;
    return checkpoint_;
}

MillenniumDosSibmTimerCheckpoint MillenniumDosSibmTimerSession::checkpoint() const {
    return checkpoint_;
}

} // namespace eon
