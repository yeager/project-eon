#include "engine/millennium_dos_video_function_zero_session.hpp"

#include "data/sha256.hpp"

#include <limits>
#include <stdexcept>

namespace eon {

MillenniumDosVideoFunctionZeroSession::MillenniumDosVideoFunctionZeroSession(
    const std::span<const std::uint8_t> english_driver,
    const MillenniumDosVideoDriverKind kind, const std::uint8_t initial_cached_mode)
    : driver_(parse_millennium_dos_video_driver(english_driver, kind)),
      state_(initial_cached_mode == driver_.function_zero_cached_mode_unknown_sentinel
          ? MillenniumDosVideoFunctionZeroState::awaiting_cached_mode_query_result
          : MillenniumDosVideoFunctionZeroState::awaiting_set_mode_result) {
    const bool ega = kind == MillenniumDosVideoDriverKind::ega640;
    // End at the first byte of the successful continuation. The continuation
    // itself is never decoded or executed by this session.
    constexpr auto span_size = static_cast<std::size_t>(0x24);
    const auto span_offset = static_cast<std::size_t>(driver_.function_zero_address);
    const auto span_hash = ega
        ? "457990d045815f27b50e45c0e26a7e7b1cbe18d754a20fabcab065f340142c4d"
        : "fb21e417ebf59d096edf515db6258423a2e304ce513b125a075e15f0a23723e8";
    if (span_offset > english_driver.size() || english_driver.size() - span_offset < span_size
        || to_hex(sha256(english_driver.subspan(span_offset, span_size))) != span_hash) {
        throw std::runtime_error("Unsupported Millennium DOS function-zero instruction span");
    }
    if (driver_.function_zero_cached_mode_store_instruction
            != driver_.function_zero_address + 0x10
        || driver_.function_zero_cached_mode_query_interrupt_site
            != driver_.function_zero_address + 0x0e
        || driver_.function_zero_set_mode_interrupt_site != driver_.function_zero_address + 0x16
        || driver_.function_zero_verify_mode_interrupt_site != driver_.function_zero_address + 0x1a
        || driver_.function_zero_mode_mismatch_zero_ax_instruction
            != driver_.function_zero_address + 0x20
        || driver_.function_zero_mode_mismatch_return != driver_.function_zero_address + 0x22
        || driver_.function_zero_mode_match_branch_target != driver_.function_zero_address + 0x23) {
        throw std::runtime_error("Unsupported Millennium DOS function-zero instruction profile");
    }
}

std::optional<MillenniumDosVideoFunctionZeroBoundary>
MillenniumDosVideoFunctionZeroSession::boundary() const {
    switch (state_) {
    case MillenniumDosVideoFunctionZeroState::awaiting_cached_mode_query_result:
        // Only AH is set before this INT 10h. AL remains caller-owned.
        return MillenniumDosVideoFunctionZeroBoundary{
            MillenniumDosVideoFunctionZeroBoundaryKind::cached_mode_query, next_sequence_,
            driver_.function_zero_cached_mode_query_interrupt_site, 0x10, 0x0f00, 0xff00};
    case MillenniumDosVideoFunctionZeroState::awaiting_set_mode_result:
        return MillenniumDosVideoFunctionZeroBoundary{
            MillenniumDosVideoFunctionZeroBoundaryKind::set_mode, next_sequence_,
            driver_.function_zero_set_mode_interrupt_site, 0x10,
            driver_.function_zero_video_mode, 0xffff};
    case MillenniumDosVideoFunctionZeroState::awaiting_verify_mode_result: {
        if (!set_mode_result_al_) {
            throw std::runtime_error("Missing Millennium DOS function-zero set-mode result");
        }
        const auto al = *set_mode_result_al_;
        return MillenniumDosVideoFunctionZeroBoundary{
            MillenniumDosVideoFunctionZeroBoundaryKind::verify_mode_query, next_sequence_,
            driver_.function_zero_verify_mode_interrupt_site, 0x10,
            static_cast<std::uint16_t>(0x0f00 | al), 0xffff};
    }
    case MillenniumDosVideoFunctionZeroState::mode_match_continuation_boundary:
    case MillenniumDosVideoFunctionZeroState::mode_mismatch_ret_boundary:
        return std::nullopt;
    }
    throw std::runtime_error("Invalid Millennium DOS function-zero state");
}

void MillenniumDosVideoFunctionZeroSession::observe_bios_result(
    const MillenniumDosVideoFunctionZeroBiosResult& result) {
    const auto expected = boundary();
    if (!expected || result.sequence != expected->sequence
        || result.instruction_address != expected->instruction_address
        || result.interrupt_number != expected->interrupt_number
        || next_sequence_ == std::numeric_limits<std::uint64_t>::max()) {
        throw std::runtime_error("Detached Millennium DOS function-zero INT 10h result");
    }

    // Append only after all identity checks. Following mutations are
    // non-throwing, so rejected or reordered observations are transactional.
    bios_results_.push_back(result);
    switch (state_) {
    case MillenniumDosVideoFunctionZeroState::awaiting_cached_mode_query_result:
        cache_write_ = MillenniumDosVideoFunctionZeroCacheWrite{
            driver_.function_zero_cached_mode_store_instruction,
            driver_.function_zero_cached_mode_address,
            static_cast<std::uint8_t>(result.ax & 0xff)};
        state_ = MillenniumDosVideoFunctionZeroState::awaiting_set_mode_result;
        break;
    case MillenniumDosVideoFunctionZeroState::awaiting_set_mode_result:
        // The next instruction sets AH=$0f but leaves AL from this raw result.
        set_mode_result_al_ = static_cast<std::uint8_t>(result.ax & 0xff);
        state_ = MillenniumDosVideoFunctionZeroState::awaiting_verify_mode_result;
        break;
    case MillenniumDosVideoFunctionZeroState::awaiting_verify_mode_result:
        if (static_cast<std::uint8_t>(result.ax & 0xff) == driver_.function_zero_video_mode) {
            state_ = MillenniumDosVideoFunctionZeroState::mode_match_continuation_boundary;
            outcome_ = MillenniumDosVideoFunctionZeroOutcome{
                MillenniumDosVideoFunctionZeroEndpoint::mode_match_continuation,
                driver_.function_zero_mode_match_branch_target, result, result.ax, false};
        } else {
            state_ = MillenniumDosVideoFunctionZeroState::mode_mismatch_ret_boundary;
            outcome_ = MillenniumDosVideoFunctionZeroOutcome{
                MillenniumDosVideoFunctionZeroEndpoint::mode_mismatch_ret_boundary,
                driver_.function_zero_mode_mismatch_return, result, 0, false};
        }
        break;
    case MillenniumDosVideoFunctionZeroState::mode_match_continuation_boundary:
    case MillenniumDosVideoFunctionZeroState::mode_mismatch_ret_boundary:
        throw std::runtime_error("Millennium DOS function-zero session has stopped");
    }
    ++next_sequence_;
}

} // namespace eon
