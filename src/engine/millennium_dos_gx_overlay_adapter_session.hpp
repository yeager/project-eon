#pragma once

#include <cstdint>
#include <span>

namespace eon {

enum class MillenniumDosGxOverlayAdapterState {
    awaiting_segment,
    awaiting_far_transfer,
    awaiting_overlay_return,
    awaiting_caller_return,
    returned,
};

struct MillenniumDosGxOverlayAdapterBoundary {
    std::uint16_t instruction_address = 0;
    std::uint16_t segment = 0;
    std::uint16_t offset = 0;
    constexpr bool operator==(const MillenniumDosGxOverlayAdapterBoundary&) const = default;
};

// Exact instruction-bound model of 2200AD.EXE $6c52..$6c72. The session
// observes the resident overlay segment and transfers; it never executes the
// overlay, supplies a segment, or interprets AX.
class MillenniumDosGxOverlayAdapterSession {
public:
    MillenniumDosGxOverlayAdapterSession(std::span<const std::uint8_t> game_executable,
        std::uint16_t caller_ax, std::uint16_t caller_return, std::uint16_t code_segment);

    [[nodiscard]] MillenniumDosGxOverlayAdapterState state() const { return state_; }
    [[nodiscard]] MillenniumDosGxOverlayAdapterBoundary boundary() const;
    [[nodiscard]] std::uint16_t caller_ax() const { return caller_ax_; }
    [[nodiscard]] std::uint16_t overlay_segment() const { return overlay_segment_; }

    void observe_segment(std::uint16_t instruction, std::uint16_t address, std::uint16_t value);
    void observe_far_transfer(std::uint16_t instruction, std::uint16_t segment,
        std::uint16_t offset);
    void observe_overlay_return(std::uint16_t instruction, std::uint16_t segment,
        std::uint16_t offset);
    void observe_caller_return(std::uint16_t instruction, std::uint16_t segment,
        std::uint16_t destination);

private:
    MillenniumDosGxOverlayAdapterState state_ =
        MillenniumDosGxOverlayAdapterState::awaiting_segment;
    std::uint16_t caller_ax_ = 0;
    std::uint16_t caller_return_ = 0;
    std::uint16_t code_segment_ = 0;
    std::uint16_t overlay_segment_ = 0;
};

} // namespace eon
