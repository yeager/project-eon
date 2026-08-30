#pragma once

#include "data/millennium_dos_game_flow.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>

namespace eon {

enum class MillenniumDosGxStartupSessionState {
    awaiting_private_return,
    awaiting_mode_byte,
    awaiting_adapter_return,
    awaiting_post_overlay_call_returns,
    awaiting_post_overlay_mode_byte,
    post_overlay_private_interrupt_boundary,
};

// Trace-gated execution of the call-free GX suffix. It owns no original bytes
// and never models DOS, the private interrupt, or the GX adapter.
class MillenniumDosGxStartupSession {
public:
    MillenniumDosGxStartupSession(std::span<const std::uint8_t> game_executable,
        std::span<const std::uint8_t> gx_overlay_executable);
    void observe_private_return(std::uint16_t ax);
    void observe_mode_byte(std::uint8_t value);
    void observe_adapter_return();
    // The six calls at $d376..$d385 remain opaque. A trace may advance across
    // each one only after it has observed that particular CALL return.
    void observe_post_overlay_call_return();
    // This is a new observation, rather than a reuse of the earlier selector
    // byte: the original rereads $da05 after six opaque calls.
    void observe_post_overlay_mode_byte(std::uint8_t value);
    [[nodiscard]] MillenniumDosGxStartupSessionState state() const { return state_; }
    [[nodiscard]] const std::optional<MillenniumDosGxOverlayStartupEvaluation>& evaluation() const { return evaluation_; }
    [[nodiscard]] const std::optional<MillenniumDosPostOverlayContinuationEvaluation>&
    post_overlay_evaluation() const { return post_overlay_evaluation_; }
    [[nodiscard]] std::size_t observed_post_overlay_call_return_count() const {
        return observed_post_overlay_call_return_count_;
    }
    [[nodiscard]] std::optional<std::uint8_t> overlay_byte(std::uint16_t offset) const;
private:
    void require_state(MillenniumDosGxStartupSessionState expected, const char* observation) const;
    void apply_overlay_writes(const MillenniumDosGxOverlayStartupEvaluation& evaluation);
    std::span<const std::uint8_t> game_executable_;
    std::span<const std::uint8_t> gx_overlay_executable_;
    MillenniumDosGxStartupSessionState state_ = MillenniumDosGxStartupSessionState::awaiting_private_return;
    std::optional<std::uint16_t> private_return_ax_;
    std::optional<std::uint8_t> mode_byte_;
    std::optional<MillenniumDosGxOverlayStartupEvaluation> evaluation_;
    std::array<bool, 6> observed_post_overlay_call_returns_{};
    std::size_t observed_post_overlay_call_return_count_ = 0;
    std::optional<MillenniumDosPostOverlayContinuationEvaluation> post_overlay_evaluation_;
    std::map<std::uint16_t, std::uint8_t> overlay_bytes_;
};

} // namespace eon
