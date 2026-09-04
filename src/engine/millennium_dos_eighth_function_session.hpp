#pragma once

#include "data/millennium_dos_game_flow.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace eon {

enum class MillenniumDosEighthFunctionState {
    awaiting_preflight_call_return,
    awaiting_wait_call_return,
    awaiting_wait_bl,
    returned,
};

enum class MillenniumDosEighthFunctionBoundaryKind { call_return, register_bl, local_return };

struct MillenniumDosEighthFunctionBoundary {
    MillenniumDosEighthFunctionBoundaryKind kind =
        MillenniumDosEighthFunctionBoundaryKind::call_return;
    std::uint16_t instruction_address = 0;
    std::optional<std::uint16_t> call_target;
    std::optional<std::uint16_t> known_ax;
    std::size_t wait_iteration = 0;
    constexpr bool operator==(const MillenniumDosEighthFunctionBoundary&) const = default;
};

struct MillenniumDosEighthFunctionByteEffect {
    std::uint16_t instruction_address = 0;
    std::uint16_t runtime_address = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumDosEighthFunctionByteEffect&) const = default;
};

// Exact manual recompilation of $7306..$7319. The called $731a preflight is
// deliberately opaque here even though separate static evaluators exist.
class MillenniumDosEighthFunctionSession {
public:
    explicit MillenniumDosEighthFunctionSession(std::span<const std::uint8_t> executable);

    [[nodiscard]] MillenniumDosEighthFunctionState state() const { return state_; }
    [[nodiscard]] MillenniumDosEighthFunctionBoundary boundary() const;
    [[nodiscard]] const std::vector<MillenniumDosEighthFunctionByteEffect>& effects() const {
        return effects_;
    }
    [[nodiscard]] const std::vector<std::uint8_t>& shifted_bl_values() const {
        return shifted_bl_values_;
    }
    void observe_call_return(std::uint16_t call_address, std::uint16_t return_address);
    void observe_bl(std::uint16_t shift_address, std::uint8_t value);

private:
    MillenniumDosEighthFunctionKeyTrace trace_;
    MillenniumDosEighthFunctionState state_ =
        MillenniumDosEighthFunctionState::awaiting_preflight_call_return;
    std::size_t wait_iteration_ = 0;
    std::vector<MillenniumDosEighthFunctionByteEffect> effects_;
    std::vector<std::uint8_t> shifted_bl_values_;
};

} // namespace eon
