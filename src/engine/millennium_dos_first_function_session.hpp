#pragma once

#include "data/millennium_dos_game_flow.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace eon {

enum class MillenniumDosFirstFunctionState {
    awaiting_display_return,
    awaiting_setup_first_return,
    awaiting_setup_second_return,
    awaiting_terminal_return,
    awaiting_wait_return,
    awaiting_wait_bl,
    returned,
};
enum class MillenniumDosFirstFunctionBoundaryKind { call_return, register_bl, local_return };
struct MillenniumDosFirstFunctionBoundary {
    MillenniumDosFirstFunctionBoundaryKind kind = MillenniumDosFirstFunctionBoundaryKind::call_return;
    std::uint16_t instruction_address = 0;
    std::optional<std::uint16_t> call_target;
    std::optional<std::uint16_t> known_ax;
    std::size_t wait_iteration = 0;
    constexpr bool operator==(const MillenniumDosFirstFunctionBoundary&) const = default;
};
struct MillenniumDosFirstFunctionEffect {
    std::uint16_t instruction_address = 0;
    std::uint16_t runtime_address = 0;
    std::uint8_t width = 0;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosFirstFunctionEffect&) const = default;
};

class MillenniumDosFirstFunctionSession {
public:
    explicit MillenniumDosFirstFunctionSession(std::span<const std::uint8_t> executable);
    [[nodiscard]] MillenniumDosFirstFunctionState state() const { return state_; }
    [[nodiscard]] MillenniumDosFirstFunctionBoundary boundary() const;
    [[nodiscard]] const std::vector<MillenniumDosFirstFunctionEffect>& effects() const {
        return effects_;
    }
    void observe_call_return(std::uint16_t call, std::uint16_t returned_to);
    void observe_bl(std::uint16_t instruction, std::uint8_t value);
private:
    void enter_setup();
    MillenniumDosFirstFunctionKeyTrace trace_;
    MillenniumDosFirstFunctionState state_ = MillenniumDosFirstFunctionState::awaiting_display_return;
    std::size_t wait_iteration_ = 0;
    std::vector<MillenniumDosFirstFunctionEffect> effects_;
};
} // namespace eon
