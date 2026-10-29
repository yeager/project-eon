#pragma once

#include "data/millennium_dos_sound_driver.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace eon {

enum class MillenniumDosSibmFunctionZeroState : std::uint8_t {
    awaiting_first_call,
    awaiting_timer_vector_get,
    awaiting_timer_vector_set,
    timer_vector_set_return_observed,
    rejected,
};

struct MillenniumDosSibmFunctionZeroMemoryEffect {
    std::uint16_t address = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumDosSibmFunctionZeroMemoryEffect&) const = default;
};

struct MillenniumDosSibmFunctionZeroCheckpoint {
    MillenniumDosSibmFunctionZeroState state =
        MillenniumDosSibmFunctionZeroState::awaiting_first_call;
    std::optional<std::uint16_t> function_input;
    std::uint16_t next_instruction_address = 0x04a8;
    std::optional<std::uint16_t> function_zero_ax;
    std::optional<std::uint16_t> dos_ax;
    std::optional<std::uint16_t> dos_interrupt_instruction;
    std::optional<std::uint16_t> timer_vector_offset;
    std::optional<std::uint16_t> timer_vector_segment;
    std::optional<std::uint16_t> timer_vector_return_ax;
    std::optional<std::uint16_t> timer_vector_set_return_ax;
    std::optional<std::uint16_t> timer_vector_set_return_bx;
    std::optional<std::uint16_t> timer_vector_set_return_cx;
    std::optional<std::uint16_t> timer_vector_set_return_dx;
    std::optional<std::uint16_t> timer_vector_set_return_flags;
    std::optional<std::uint16_t> timer_vector_set_return_es;
    std::uint64_t last_observation_sequence = 0;
    bool software_speaker_gate_disabled = false;
    std::uint16_t software_current_divisor = 0;
    std::uint16_t timer_handler_address = 0;
    std::vector<MillenniumDosSibmFunctionZeroMemoryEffect> memory_effects;
    std::string error;
};

// Executes only the deterministic first-call portion of SIBM function zero.
// DOS AH=$35/$25 and physical PIT/speaker effects are explicit hard boundaries.
class MillenniumDosSibmFunctionZeroSession {
public:
    MillenniumDosSibmFunctionZeroSession(
        std::span<const std::uint8_t> game_executable,
        std::span<const std::uint8_t> sibm_driver);

    [[nodiscard]] MillenniumDosSibmFunctionZeroCheckpoint execute_local_prefix(
        std::uint16_t function_input);
    // Executes the hash-locked `$d07a` mode selection and then the proven
    // deterministic prefix of SIBM function zero. Mode 1 selects `$0380`;
    // every other byte selects `$0220`, matching the original comparisons.
    [[nodiscard]] MillenniumDosSibmFunctionZeroCheckpoint
    execute_startup_helper_prefix(std::uint8_t mode_byte);
    // Accepts only the raw DOS AH=$35 return at SIBM:$0233. The result is
    // stored at the hash-identified driver cells and the exact AH=$25 request
    // at $0246 is published; no vector installation is inferred.
    [[nodiscard]] MillenniumDosSibmFunctionZeroCheckpoint
    observe_timer_vector_get_return(std::uint64_t sequence,
        std::uint16_t instruction_address, std::uint16_t return_instruction_pointer,
        std::uint16_t ax, std::uint16_t bx, std::uint16_t es);
    // Retains the complete DOS AH=$25 return at `$0246` and stops at the
    // helper's first stack restore. It does not assert IVT memory contents or
    // consume the helper's unknown saved DS/ES words.
    [[nodiscard]] MillenniumDosSibmFunctionZeroCheckpoint
    observe_timer_vector_set_return(std::uint64_t sequence,
        std::uint16_t instruction_address, std::uint16_t return_instruction_pointer,
        std::uint16_t ax, std::uint16_t bx, std::uint16_t cx, std::uint16_t dx,
        std::uint16_t flags, std::uint16_t es);
    [[nodiscard]] MillenniumDosSibmFunctionZeroCheckpoint checkpoint() const;

private:
    MillenniumDosSibmStartupProfile profile_;
    std::uint8_t initial_driver_flags_ = 0;
    std::uint8_t initial_initialized_flag_ = 0;
    std::uint16_t initial_current_divisor_ = 0;
    MillenniumDosSibmFunctionZeroCheckpoint checkpoint_;
};

} // namespace eon
