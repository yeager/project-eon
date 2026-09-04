#pragma once

#include "data/millennium_dos_game_flow.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace eon {

enum class MillenniumDosTenthFunctionState {
    awaiting_initialization_guard,
    awaiting_display_call_return,
    awaiting_enabled_byte,
    awaiting_enabled_call_return,
    awaiting_limit_byte,
    awaiting_preflight_call_return,
    awaiting_conditional_byte,
    awaiting_conditional_call_return,
    awaiting_terminal_call_return,
    awaiting_wait_byte,
    awaiting_wait_call_return,
    awaiting_wait_zero_flag,
    awaiting_wait_bl,
    awaiting_final_call_return,
    awaiting_busy_guard,
    awaiting_busy_optional_final_call_return,
    awaiting_busy_call_return,
    awaiting_busy_word,
    awaiting_busy_dx_call_return,
    awaiting_busy_last_call_return,
    returned_by_guard,
    returned,
};

enum class MillenniumDosTenthFunctionBoundaryKind {
    runtime_word,
    runtime_byte,
    call_return,
    zero_flag,
    register_bl,
    local_return,
};

struct MillenniumDosTenthFunctionBoundary {
    MillenniumDosTenthFunctionBoundaryKind kind =
        MillenniumDosTenthFunctionBoundaryKind::runtime_word;
    std::uint16_t instruction_address = 0;
    std::optional<std::uint16_t> runtime_address;
    std::optional<std::uint32_t> call_target;
    std::size_t loop_iteration = 0;
    constexpr bool operator==(const MillenniumDosTenthFunctionBoundary&) const = default;
};

struct MillenniumDosTenthFunctionByteEffect {
    std::uint16_t address = 0;
    std::optional<std::uint8_t> previous;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumDosTenthFunctionByteEffect&) const = default;
};

// Typed manual recompilation of the exact English $7384..$740e handler.
// The raw dispatch value and every native value/call return remain external
// observations. No field assigns a gameplay meaning to the handler.
class MillenniumDosTenthFunctionSession {
public:
    explicit MillenniumDosTenthFunctionSession(
        std::span<const std::uint8_t> game_executable);

    [[nodiscard]] MillenniumDosTenthFunctionState state() const { return state_; }
    [[nodiscard]] MillenniumDosTenthFunctionBoundary boundary() const;
    [[nodiscard]] const std::vector<MillenniumDosTenthFunctionByteEffect>&
    runtime_effects() const { return runtime_effects_; }
    [[nodiscard]] std::size_t limit_loop_count() const { return limit_loop_count_; }
    [[nodiscard]] std::size_t wait_loop_count() const { return wait_loop_count_; }

    void observe_runtime_word(std::uint16_t instruction_address,
        std::uint16_t runtime_address, std::uint16_t value);
    void observe_runtime_byte(std::uint16_t instruction_address,
        std::uint16_t runtime_address, std::uint8_t value);
    void observe_call_return(std::uint16_t call_address,
        std::uint16_t return_address);
    void observe_zero_flag(std::uint16_t branch_address, bool set);
    void observe_bl(std::uint16_t shift_address, std::uint8_t value);

private:
    void enter_call(MillenniumDosTenthFunctionState state,
        std::uint16_t address, std::uint32_t target);
    void enter_terminal_chain(std::uint16_t address, std::uint32_t target);
    void enter_busy_chain(std::uint16_t address, std::uint32_t target,
        MillenniumDosTenthFunctionState state);
    void record_write(std::uint16_t address, std::uint8_t value);

    std::span<const std::uint8_t> game_executable_;
    MillenniumDosTenthFunctionKeyTrace trace_;
    MillenniumDosTenthFunctionState state_ =
        MillenniumDosTenthFunctionState::awaiting_initialization_guard;
    std::uint16_t call_address_ = 0;
    std::uint32_t call_target_ = 0;
    std::size_t limit_loop_count_ = 0;
    std::size_t wait_loop_count_ = 0;
    std::vector<MillenniumDosTenthFunctionByteEffect> runtime_effects_;
    std::optional<std::uint8_t> local_mode_;
};

} // namespace eon
