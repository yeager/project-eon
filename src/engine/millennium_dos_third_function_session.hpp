#pragma once

#include "data/millennium_dos_game_flow.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace eon {

enum class MillenniumDosThirdFunctionState {
    awaiting_initialization_guard,
    awaiting_availability_word,
    awaiting_wait_call_return,
    awaiting_wait_bl,
    awaiting_first_setup_call_return,
    awaiting_second_setup_call_return,
    awaiting_setup_count_word,
    source_far_pointer_boundary,
    returned_by_guard,
    returned_by_wait,
};

enum class MillenniumDosThirdFunctionBoundaryKind {
    runtime_word, call_return, register_bl, far_pointer, local_return,
};

struct MillenniumDosThirdFunctionBoundary {
    MillenniumDosThirdFunctionBoundaryKind kind =
        MillenniumDosThirdFunctionBoundaryKind::runtime_word;
    std::uint16_t instruction_address = 0;
    std::optional<std::uint16_t> runtime_address;
    std::optional<std::uint16_t> call_target;
    std::optional<std::uint16_t> known_ax;
    std::size_t wait_iteration = 0;
    constexpr bool operator==(const MillenniumDosThirdFunctionBoundary&) const = default;
};

struct MillenniumDosThirdFunctionEffect {
    std::uint16_t instruction_address = 0;
    std::uint16_t runtime_address = 0;
    std::uint8_t width = 0;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosThirdFunctionEffect&) const = default;
};

class MillenniumDosThirdFunctionSession {
public:
    explicit MillenniumDosThirdFunctionSession(std::span<const std::uint8_t> executable);
    [[nodiscard]] MillenniumDosThirdFunctionState state() const { return state_; }
    [[nodiscard]] MillenniumDosThirdFunctionBoundary boundary() const;
    [[nodiscard]] const std::vector<MillenniumDosThirdFunctionEffect>& effects() const {
        return effects_;
    }
    void observe_runtime_word(std::uint16_t instruction, std::uint16_t address,
        std::uint16_t value);
    void observe_call_return(std::uint16_t call, std::uint16_t returned_to);
    void observe_bl(std::uint16_t instruction, std::uint8_t value);
private:
    MillenniumDosThirdFunctionKeyTrace trace_;
    MillenniumDosThirdFunctionState state_ =
        MillenniumDosThirdFunctionState::awaiting_initialization_guard;
    std::size_t wait_iteration_ = 0;
    std::vector<MillenniumDosThirdFunctionEffect> effects_;
};

} // namespace eon
