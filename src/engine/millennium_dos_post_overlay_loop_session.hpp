#pragma once

#include "data/millennium_dos_game_flow.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace eon {

enum class MillenniumDosPostOverlayLoopState {
    awaiting_private_interrupt_return,
    palette_bios_interrupt_boundary,
    awaiting_call_return,
    awaiting_first_al,
    awaiting_toggle_source_byte,
    awaiting_action_al,
    awaiting_dispatch_guard_byte,
    dispatch_call_boundary,
};

enum class MillenniumDosPostOverlayLoopBoundaryKind {
    private_interrupt,
    bios_interrupt,
    call_return,
    register_value,
    runtime_byte,
    dispatch_call,
};

struct MillenniumDosPostOverlayLoopBoundary {
    MillenniumDosPostOverlayLoopBoundaryKind kind =
        MillenniumDosPostOverlayLoopBoundaryKind::private_interrupt;
    std::uint16_t instruction_address = 0;
    std::optional<std::uint16_t> call_target;
    std::optional<std::uint16_t> runtime_address;
    std::optional<std::uint8_t> interrupt;
    std::size_t call_ordinal = 0;
    constexpr bool operator==(const MillenniumDosPostOverlayLoopBoundary&) const = default;
};

struct MillenniumDosPostOverlayRuntimeByteEffect {
    std::uint16_t address = 0;
    std::uint8_t previous = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumDosPostOverlayRuntimeByteEffect&) const = default;
};

// A manually recompiled continuation after the post-overlay INT 91h return.
// The selected mode is the already observed $da05 value from the preceding
// typed process boundary. No caller, interrupt, call, or input is invented.
class MillenniumDosPostOverlayLoopSession {
public:
    MillenniumDosPostOverlayLoopSession(std::span<const std::uint8_t> game_executable,
        std::uint8_t selected_mode_byte);

    [[nodiscard]] MillenniumDosPostOverlayLoopState state() const { return state_; }
    [[nodiscard]] MillenniumDosPostOverlayLoopBoundary boundary() const;
    [[nodiscard]] std::size_t completed_call_return_count() const {
        return completed_call_return_count_;
    }
    [[nodiscard]] std::size_t action_poll_count() const { return action_poll_count_; }
    [[nodiscard]] std::optional<std::uint8_t> observed_action() const {
        return observed_action_;
    }
    [[nodiscard]] std::optional<std::size_t> function_key_index() const {
        return function_key_index_;
    }
    [[nodiscard]] const std::vector<MillenniumDosPostOverlayRuntimeByteEffect>&
    runtime_effects() const { return runtime_effects_; }

    void observe_private_interrupt_return(std::uint16_t interrupt_address,
        std::uint16_t ax);
    void observe_call_return(std::uint16_t call_address, std::uint16_t return_address);
    void observe_al(std::uint16_t test_address, std::uint8_t value);
    void observe_runtime_byte(std::uint16_t load_address,
        std::uint16_t runtime_address, std::uint8_t value);

private:
    void enter_call(std::size_t ordinal);
    void finish_dispatch(std::uint16_t call_address, std::uint16_t call_target,
        std::optional<std::size_t> function_key_index = std::nullopt);

    std::span<const std::uint8_t> game_executable_;
    MillenniumDosPostOverlayAdapterLoop loop_;
    MillenniumDosPostOverlayDispatchPrefix dispatch_;
    std::uint8_t selected_mode_byte_ = 0;
    MillenniumDosPostOverlayLoopState state_ =
        MillenniumDosPostOverlayLoopState::awaiting_private_interrupt_return;
    std::size_t call_ordinal_ = 0;
    std::size_t completed_call_return_count_ = 0;
    std::size_t action_poll_count_ = 0;
    std::optional<std::uint8_t> observed_action_;
    std::optional<std::size_t> function_key_index_;
    std::optional<std::uint16_t> dispatch_call_address_;
    std::optional<std::uint16_t> dispatch_call_target_;
    std::vector<MillenniumDosPostOverlayRuntimeByteEffect> runtime_effects_;
};

} // namespace eon
