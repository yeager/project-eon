#pragma once

#include "data/millennium_dos_game_flow.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace eon {

enum class MillenniumDosSixthFunctionState {
    awaiting_initialization_guard,
    awaiting_display_call_return,
    awaiting_command_call_return,
    awaiting_second_call_return,
    awaiting_first_byte,
    awaiting_second_byte,
    awaiting_word,
    awaiting_wait_call_return,
    awaiting_wait_bl,
    returned_by_guard,
    returned,
    restoration_caller_call_return,
    caller_helper_first_call_return,
    caller_helper_second_call_return,
    caller_helper_third_call_return,
    caller_helper_fourth_call_return,
    caller_helper_far_offset,
    caller_helper_far_segment,
    caller_helper_saved_byte,
    caller_helper_external_continuation,
    restoration_first_call_return,
    restoration_second_call_return,
    restoration_third_call_return,
    restoration_fourth_call_return,
    restoration_runtime_byte,
    restoration_fifth_call_return,
    restoration_sixth_call_return,
    restoration_seventh_call_return,
    restoration_eighth_call_return,
    restoration_ninth_call_return,
    restoration_tenth_call_return,
    restoration_eleventh_call_return,
    restoration_twelfth_call_return,
    restoration_returned,
};

enum class MillenniumDosSixthFunctionBoundaryKind {
    runtime_word,
    runtime_byte,
    call_return,
    register_bl,
    local_return,
    external_continuation,
};

struct MillenniumDosSixthFunctionBoundary {
    MillenniumDosSixthFunctionBoundaryKind kind =
        MillenniumDosSixthFunctionBoundaryKind::runtime_word;
    std::uint16_t instruction_address = 0;
    std::optional<std::uint16_t> runtime_address;
    std::optional<std::uint16_t> call_target;
    std::optional<std::uint16_t> known_ax;
    std::size_t wait_iteration = 0;
    constexpr bool operator==(const MillenniumDosSixthFunctionBoundary&) const = default;
};

struct MillenniumDosSixthFunctionEffect {
    std::uint16_t address = 0;
    std::uint8_t width = 0;
    std::optional<std::uint16_t> previous;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosSixthFunctionEffect&) const = default;
};

struct MillenniumDosSixthFunctionFarClearEffect {
    std::uint16_t segment = 0;
    std::uint16_t offset = 0;
    std::uint16_t word_count = 0;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosSixthFunctionFarClearEffect&) const = default;
};

struct MillenniumDosSixthFunctionStateClearEffect {
    std::uint16_t offset = 0;
    std::uint16_t byte_count = 0;
    std::uint8_t value = 0;
    std::uint16_t preserved_offset = 0;
    std::uint8_t preserved_value = 0;
    constexpr bool operator==(const MillenniumDosSixthFunctionStateClearEffect&) const = default;
};

// Typed manual recompilation of the exact English $7415..$7454 handler and
// its separately entered complete $7455..$74aa restoration routine.
class MillenniumDosSixthFunctionSession {
public:
    explicit MillenniumDosSixthFunctionSession(
        std::span<const std::uint8_t> game_executable);

    [[nodiscard]] MillenniumDosSixthFunctionState state() const { return state_; }
    [[nodiscard]] MillenniumDosSixthFunctionBoundary boundary() const;
    [[nodiscard]] const std::vector<MillenniumDosSixthFunctionEffect>&
    effects() const { return effects_; }
    [[nodiscard]] const std::vector<std::uint8_t>& shifted_bl_values() const {
        return shifted_bl_values_;
    }
    [[nodiscard]] std::optional<std::uint16_t> caller_helper_far_offset() const {
        return caller_helper_far_offset_;
    }
    [[nodiscard]] std::optional<std::uint16_t> caller_helper_far_segment() const {
        return caller_helper_far_segment_;
    }
    [[nodiscard]] std::optional<MillenniumDosSixthFunctionFarClearEffect>
    caller_helper_far_clear_effect() const { return caller_helper_far_clear_effect_; }
    [[nodiscard]] std::optional<std::uint8_t> caller_helper_saved_byte() const {
        return caller_helper_saved_byte_;
    }
    [[nodiscard]] std::optional<MillenniumDosSixthFunctionStateClearEffect>
    caller_helper_state_clear_effect() const { return caller_helper_state_clear_effect_; }

    void observe_runtime_word(std::uint16_t instruction_address,
        std::uint16_t runtime_address, std::uint16_t value);
    void observe_runtime_byte(std::uint16_t instruction_address,
        std::uint16_t runtime_address, std::uint8_t value);
    void observe_call_return(std::uint16_t call_address,
        std::uint16_t return_address);
    void observe_bl(std::uint16_t shift_address, std::uint8_t value);
    void begin_restoration();
    void begin_restoration_caller_helper_prefix();

private:
    void enter_call(MillenniumDosSixthFunctionState state,
        std::uint16_t address, std::uint16_t target,
        std::optional<std::uint16_t> known_ax = std::nullopt);
    void record_effect(std::uint16_t address, std::uint8_t width,
        std::optional<std::uint16_t> previous, std::uint16_t value);

    MillenniumDosSixthFunctionKeyTrace trace_;
    MillenniumDosSixthFunctionState state_ =
        MillenniumDosSixthFunctionState::awaiting_initialization_guard;
    std::uint16_t call_address_ = 0;
    std::uint16_t call_target_ = 0;
    std::optional<std::uint16_t> call_known_ax_;
    std::optional<std::uint8_t> first_byte_;
    std::optional<std::uint8_t> second_byte_;
    std::size_t wait_iteration_ = 0;
    std::vector<MillenniumDosSixthFunctionEffect> effects_;
    std::vector<std::uint8_t> shifted_bl_values_;
    std::optional<std::uint16_t> caller_helper_far_offset_;
    std::optional<std::uint16_t> caller_helper_far_segment_;
    std::optional<MillenniumDosSixthFunctionFarClearEffect> caller_helper_far_clear_effect_;
    std::optional<std::uint8_t> caller_helper_saved_byte_;
    std::optional<MillenniumDosSixthFunctionStateClearEffect>
        caller_helper_state_clear_effect_;
};

} // namespace eon
