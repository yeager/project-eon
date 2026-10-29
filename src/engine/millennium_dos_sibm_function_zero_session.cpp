#include "engine/millennium_dos_sibm_function_zero_session.hpp"

#include "data/sha256.hpp"

#include <array>
#include <stdexcept>
#include <utility>

namespace eon {
namespace {

constexpr std::string_view function_zero_handler_sha256 =
    "fa8089e4e17500d3b1ee5b66b882fcd1437f10772ec80fc992fe59e1f82147bf";
constexpr std::string_view speaker_reset_sha256 =
    "d21f5933a6d4c78d72fe1218d756c370f1a7b0504fa14759777631c33b527e10";
constexpr std::string_view timer_install_sha256 =
    "4fd2fb8ee42b5990f90a0d449db7486bfdb01270e499d490b2ce9ccd2e3f456d";

void require_range(const std::span<const std::uint8_t> bytes,
    const std::size_t offset, const std::size_t size, const std::string_view label,
    const std::string_view expected_sha256) {
    if (offset > bytes.size() || size > bytes.size() - offset
        || to_hex(sha256(bytes.subspan(offset, size))) != expected_sha256) {
        throw std::runtime_error("SIBM function-zero source span differs: "
            + std::string(label));
    }
}

} // namespace

MillenniumDosSibmFunctionZeroSession::MillenniumDosSibmFunctionZeroSession(
    const std::span<const std::uint8_t> game_executable,
    const std::span<const std::uint8_t> sibm_driver)
    : profile_(parse_millennium_dos_sibm_startup_profile(game_executable, sibm_driver)) {
    require_range(sibm_driver, 0x04a8, 32, "function-zero handler",
        function_zero_handler_sha256);
    require_range(sibm_driver, 0x01f6, 13, "speaker reset helper", speaker_reset_sha256);
    require_range(sibm_driver, 0x0221, 42, "timer install helper", timer_install_sha256);
    initial_driver_flags_ = sibm_driver[0x00b1];
    initial_initialized_flag_ = sibm_driver[0x00cd];
    initial_current_divisor_ = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(sibm_driver[0x00cf])
        | (static_cast<std::uint16_t>(sibm_driver[0x00d0]) << 8U));
}

MillenniumDosSibmFunctionZeroCheckpoint
MillenniumDosSibmFunctionZeroSession::execute_local_prefix(
    const std::uint16_t function_input) {
    if (checkpoint_.state != MillenniumDosSibmFunctionZeroState::awaiting_first_call) {
        checkpoint_.state = MillenniumDosSibmFunctionZeroState::rejected;
        checkpoint_.error = "SIBM function-zero prefix is one-shot";
        return checkpoint_;
    }
    if ((function_input != profile_.function_zero_input_for_mode_one
            && function_input != profile_.function_zero_input_for_other_modes)
        || initial_initialized_flag_ != 0
        || (initial_driver_flags_ & 0x30U) != 0
        || initial_current_divisor_ == function_input) {
        checkpoint_.state = MillenniumDosSibmFunctionZeroState::rejected;
        checkpoint_.error = "SIBM function-zero input or initial driver state is not admitted";
        return checkpoint_;
    }
    checkpoint_.function_input = function_input;

    // `$04a8` sets the initialized byte, then calls `$01f6` and `$0221`.
    // The software audio state records only the net bit-clear/divisor-zero
    // result of `$01f6`; it does not read or write a host I/O port.
    checkpoint_.memory_effects = {
        {0x00cd, 1},
        {0x00cf, 0},
        {0x00d0, 0},
        {0x00b1, static_cast<std::uint8_t>(initial_driver_flags_ | 0x10U)},
    };
    checkpoint_.software_speaker_gate_disabled = true;
    checkpoint_.software_current_divisor = 0;
    checkpoint_.timer_handler_address = 0x034d;
    checkpoint_.dos_ax = 0x3508;
    checkpoint_.dos_interrupt_instruction = 0x0233;
    checkpoint_.next_instruction_address = 0x0233;
    checkpoint_.state = MillenniumDosSibmFunctionZeroState::awaiting_timer_vector_get;
    return checkpoint_;
}

MillenniumDosSibmFunctionZeroCheckpoint
MillenniumDosSibmFunctionZeroSession::execute_startup_helper_prefix(
    const std::uint8_t mode_byte) {
    const auto function_input = mode_byte == 1
        ? profile_.function_zero_input_for_mode_one
        : profile_.function_zero_input_for_other_modes;
    return execute_local_prefix(function_input);
}

MillenniumDosSibmFunctionZeroCheckpoint
MillenniumDosSibmFunctionZeroSession::observe_timer_vector_get_return(
    const std::uint64_t sequence, const std::uint16_t instruction_address,
    const std::uint16_t return_instruction_pointer, const std::uint16_t ax,
    const std::uint16_t bx, const std::uint16_t es) {
    if (checkpoint_.state != MillenniumDosSibmFunctionZeroState::awaiting_timer_vector_get
        || sequence == 0 || sequence <= checkpoint_.last_observation_sequence
        || instruction_address != 0x0233 || return_instruction_pointer != 0x0235
        || checkpoint_.dos_interrupt_instruction != 0x0233
        || checkpoint_.dos_ax != 0x3508) {
        checkpoint_.state = MillenniumDosSibmFunctionZeroState::rejected;
        checkpoint_.error = "SIBM timer-vector return is detached from INT $21/AH=$35";
        return checkpoint_;
    }

    // SIBM stores BX at $0180 and ES at $0182, little-endian, before it
    // reloads DS=CS and requests INT $21/AH=$25 at $0246.
    checkpoint_.timer_vector_offset = bx;
    checkpoint_.timer_vector_segment = es;
    checkpoint_.timer_vector_return_ax = ax;
    checkpoint_.memory_effects.insert(checkpoint_.memory_effects.end(), {
        {0x0180, static_cast<std::uint8_t>(bx)},
        {0x0181, static_cast<std::uint8_t>(bx >> 8U)},
        {0x0182, static_cast<std::uint8_t>(es)},
        {0x0183, static_cast<std::uint8_t>(es >> 8U)},
    });
    checkpoint_.last_observation_sequence = sequence;
    checkpoint_.dos_ax = 0x2508;
    checkpoint_.dos_interrupt_instruction = 0x0246;
    checkpoint_.next_instruction_address = 0x0246;
    checkpoint_.state = MillenniumDosSibmFunctionZeroState::awaiting_timer_vector_set;
    return checkpoint_;
}

MillenniumDosSibmFunctionZeroCheckpoint
MillenniumDosSibmFunctionZeroSession::observe_timer_vector_set_return(
    const std::uint64_t sequence, const std::uint16_t instruction_address,
    const std::uint16_t return_instruction_pointer, const std::uint16_t ax,
    const std::uint16_t bx, const std::uint16_t cx, const std::uint16_t dx,
    const std::uint16_t flags, const std::uint16_t es) {
    if (checkpoint_.state != MillenniumDosSibmFunctionZeroState::awaiting_timer_vector_set
        || sequence == 0 || sequence <= checkpoint_.last_observation_sequence
        || instruction_address != 0x0246 || return_instruction_pointer != 0x0248
        || checkpoint_.dos_interrupt_instruction != 0x0246
        || checkpoint_.dos_ax != 0x2508 || !checkpoint_.timer_vector_offset
        || !checkpoint_.timer_vector_segment || !checkpoint_.timer_vector_return_ax) {
        checkpoint_.state = MillenniumDosSibmFunctionZeroState::rejected;
        checkpoint_.error = "SIBM timer-vector set return is detached from INT $21/AH=$25";
        return checkpoint_;
    }

    // The exact helper ends at `$024a` with POP DS, POP ES, RET. Its stack
    // preimage is not represented here, so retain the observed DOS return and
    // stop at the first restore at `$0248`.
    checkpoint_.timer_vector_set_return_ax = ax;
    checkpoint_.timer_vector_set_return_bx = bx;
    checkpoint_.timer_vector_set_return_cx = cx;
    checkpoint_.timer_vector_set_return_dx = dx;
    checkpoint_.timer_vector_set_return_flags = flags;
    checkpoint_.timer_vector_set_return_es = es;
    checkpoint_.last_observation_sequence = sequence;
    checkpoint_.next_instruction_address = 0x0248;
    checkpoint_.state = MillenniumDosSibmFunctionZeroState::timer_vector_set_return_observed;
    return checkpoint_;
}

MillenniumDosSibmFunctionZeroCheckpoint
MillenniumDosSibmFunctionZeroSession::checkpoint() const {
    return checkpoint_;
}

} // namespace eon
