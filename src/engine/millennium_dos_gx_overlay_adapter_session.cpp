#include "engine/millennium_dos_gx_overlay_adapter_session.hpp"

#include "data/millennium_dos_game_flow.hpp"

#include <stdexcept>

namespace eon {

MillenniumDosGxOverlayAdapterSession::MillenniumDosGxOverlayAdapterSession(
    const std::span<const std::uint8_t> game_executable, const std::uint16_t caller_ax,
    const std::uint16_t caller_return, const std::uint16_t code_segment)
    : caller_ax_(caller_ax), caller_return_(caller_return), code_segment_(code_segment) {
    MillenniumDosGxOverlayLoadEvidence load;
    load.loader_segment_cell_address = 0x0118;
    load.loader_entry_address = 0x11ce;
    load.caller_target = 0x11ce;
    const auto adapter = parse_millennium_dos_gx_overlay_adapter_evidence(game_executable, load);
    if (adapter.entry_address != 0x6c52 || adapter.overlay_segment_cell_address != 0x0118
        || adapter.overlay_entry_offset != 0 || adapter.far_transfer_address != 0x6c68
        || adapter.continuation_address != 0x6c69 || adapter.return_address != 0x6c72
        || caller_return_ == 0 || code_segment_ == 0) {
        throw std::runtime_error("Unsupported Millennium DOS GX adapter connection");
    }
}

MillenniumDosGxOverlayAdapterBoundary MillenniumDosGxOverlayAdapterSession::boundary() const {
    switch (state_) {
    case MillenniumDosGxOverlayAdapterState::awaiting_segment: return {0x6c60, 0, 0x0118};
    case MillenniumDosGxOverlayAdapterState::awaiting_far_transfer: return {0x6c68, overlay_segment_, 0};
    case MillenniumDosGxOverlayAdapterState::awaiting_overlay_return: return {0x0014, code_segment_, 0x6c69};
    case MillenniumDosGxOverlayAdapterState::awaiting_caller_return: return {0x6c72, code_segment_, caller_return_};
    case MillenniumDosGxOverlayAdapterState::returned: return {0x6c72, code_segment_, caller_return_};
    }
    throw std::runtime_error("Invalid Millennium DOS GX adapter state");
}

void MillenniumDosGxOverlayAdapterSession::observe_segment(const std::uint16_t instruction,
    const std::uint16_t address, const std::uint16_t value) {
    if (state_ != MillenniumDosGxOverlayAdapterState::awaiting_segment
        || instruction != 0x6c60 || address != 0x0118 || value == 0) {
        throw std::runtime_error("Detached Millennium DOS GX adapter segment");
    }
    overlay_segment_ = value;
    state_ = MillenniumDosGxOverlayAdapterState::awaiting_far_transfer;
}

void MillenniumDosGxOverlayAdapterSession::observe_far_transfer(const std::uint16_t instruction,
    const std::uint16_t segment, const std::uint16_t offset) {
    if (state_ != MillenniumDosGxOverlayAdapterState::awaiting_far_transfer
        || instruction != 0x6c68 || segment != overlay_segment_ || offset != 0) {
        throw std::runtime_error("Detached Millennium DOS GX adapter far transfer");
    }
    state_ = MillenniumDosGxOverlayAdapterState::awaiting_overlay_return;
}

void MillenniumDosGxOverlayAdapterSession::observe_overlay_return(const std::uint16_t instruction,
    const std::uint16_t segment, const std::uint16_t offset) {
    if (state_ != MillenniumDosGxOverlayAdapterState::awaiting_overlay_return
        || instruction != 0x0014 || segment != code_segment_ || offset != 0x6c69) {
        throw std::runtime_error("Detached Millennium DOS GX overlay return");
    }
    state_ = MillenniumDosGxOverlayAdapterState::awaiting_caller_return;
}

void MillenniumDosGxOverlayAdapterSession::observe_caller_return(const std::uint16_t instruction,
    const std::uint16_t segment, const std::uint16_t destination) {
    if (state_ != MillenniumDosGxOverlayAdapterState::awaiting_caller_return
        || instruction != 0x6c72 || segment != code_segment_ || destination != caller_return_) {
        throw std::runtime_error("Detached Millennium DOS GX adapter caller return");
    }
    state_ = MillenniumDosGxOverlayAdapterState::returned;
}

} // namespace eon
