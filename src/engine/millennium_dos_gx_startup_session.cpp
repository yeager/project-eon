#include "engine/millennium_dos_gx_startup_session.hpp"

#include <stdexcept>
#include <string>

namespace eon {
MillenniumDosGxStartupSession::MillenniumDosGxStartupSession(const std::span<const std::uint8_t> game,
    const std::span<const std::uint8_t> overlay) : game_executable_(game), gx_overlay_executable_(overlay) {
    const auto initial = evaluate_millennium_dos_gx_overlay_startup(game, overlay);
    if (initial.outcome != MillenniumDosGxOverlayStartupOutcome::private_interrupt_boundary || initial.boundary_address != 0x0129)
        throw std::runtime_error("Unsupported Millennium DOS GX startup initial boundary");
    evaluation_ = initial;
}
void MillenniumDosGxStartupSession::require_state(const MillenniumDosGxStartupSessionState expected, const char* const observation) const {
    if (state_ != expected) throw std::runtime_error(std::string("Out-of-order Millennium DOS GX startup observation: ") + observation);
}
void MillenniumDosGxStartupSession::observe_private_return(const std::uint16_t ax) {
    require_state(MillenniumDosGxStartupSessionState::awaiting_private_return, "private return");
    private_return_ax_ = ax; state_ = MillenniumDosGxStartupSessionState::awaiting_mode_byte;
}
void MillenniumDosGxStartupSession::observe_mode_byte(const std::uint8_t value) {
    require_state(MillenniumDosGxStartupSessionState::awaiting_mode_byte, "mode byte");
    const auto next = evaluate_millennium_dos_gx_overlay_startup(game_executable_, gx_overlay_executable_, private_return_ax_, value);
    if (next.outcome != MillenniumDosGxOverlayStartupOutcome::overlay_adapter_boundary || next.boundary_address != 0xd373)
        throw std::runtime_error("Unsupported Millennium DOS GX startup adapter boundary");
    mode_byte_ = value; evaluation_ = next; state_ = MillenniumDosGxStartupSessionState::awaiting_adapter_return;
}
void MillenniumDosGxStartupSession::apply_overlay_writes(const MillenniumDosGxOverlayStartupEvaluation& evaluation) {
    for (const auto& write : evaluation.overlay_writes) {
        if (write.byte_count != 1 && write.byte_count != 2) throw std::runtime_error("Unsupported Millennium DOS GX startup write width");
        overlay_bytes_[write.offset] = static_cast<std::uint8_t>(write.value & 0xffU);
        if (write.byte_count == 2) overlay_bytes_[static_cast<std::uint16_t>(write.offset + 1U)] = static_cast<std::uint8_t>(write.value >> 8U);
    }
}
void MillenniumDosGxStartupSession::observe_adapter_return() {
    require_state(MillenniumDosGxStartupSessionState::awaiting_adapter_return, "adapter return");
    const auto next = evaluate_millennium_dos_gx_overlay_startup(game_executable_, gx_overlay_executable_, private_return_ax_, mode_byte_, true);
    if (next.outcome != MillenniumDosGxOverlayStartupOutcome::overlay_return || next.boundary_address != 0xd376)
        throw std::runtime_error("Unsupported Millennium DOS GX startup return boundary");
    apply_overlay_writes(next); evaluation_ = next; state_ = MillenniumDosGxStartupSessionState::returned_to_caller;
}
std::optional<std::uint8_t> MillenniumDosGxStartupSession::overlay_byte(const std::uint16_t offset) const {
    const auto found = overlay_bytes_.find(offset);
    return found == overlay_bytes_.end() ? std::nullopt : std::optional<std::uint8_t>(found->second);
}
} // namespace eon
