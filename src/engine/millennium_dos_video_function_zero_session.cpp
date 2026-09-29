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
    // The primary mode-check span ends at the first successful-continuation
    // byte; a second independently hash-bound prefix is advanced only by the
    // explicit continuation step below.
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
    const auto continuation_offset = static_cast<std::size_t>(
        driver_.function_zero_mode_match_branch_target);
    const auto continuation_size = static_cast<std::size_t>(ega ? 0x12 : 0x16);
    const auto continuation_hash = ega
        ? "8dd5795f1822c4eda3f039dd2409feeafe139770e4aa782bbbbdfc70bb46cd95"
        : "a661c390d71832a868a237e975ba4a181d32e4914bd1b5bc0a86b1fb8ee4b0fc";
    if (continuation_offset > english_driver.size()
        || english_driver.size() - continuation_offset < continuation_size
        || to_hex(sha256(english_driver.subspan(continuation_offset, continuation_size)))
            != continuation_hash) {
        throw std::runtime_error("Unsupported Millennium DOS function-zero success postlude prefix");
    }
    if (ega) {
        constexpr auto stack_prefix_offset = static_cast<std::size_t>(0x1fd);
        constexpr auto stack_prefix_size = static_cast<std::size_t>(0x0a);
        constexpr auto stack_prefix_hash =
            "eea3f3e5c3ee34063ddb6aba69216ef578da41d9799d20da871fa3de40613795";
        if (stack_prefix_offset > english_driver.size()
            || english_driver.size() - stack_prefix_offset < stack_prefix_size
            || to_hex(sha256(english_driver.subspan(stack_prefix_offset, stack_prefix_size)))
                != stack_prefix_hash) {
            throw std::runtime_error("Unsupported Millennium DOS EGA success stack prefix");
        }
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
    case MillenniumDosVideoFunctionZeroState::mode_success_postlude_prefix_recorded:
    case MillenniumDosVideoFunctionZeroState::mode_success_stack_prefix_recorded:
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
        cache_write_ = MillenniumDosVideoFunctionZeroLocalWrite{
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
    case MillenniumDosVideoFunctionZeroState::mode_success_postlude_prefix_recorded:
    case MillenniumDosVideoFunctionZeroState::mode_success_stack_prefix_recorded:
    case MillenniumDosVideoFunctionZeroState::mode_mismatch_ret_boundary:
        throw std::runtime_error("Millennium DOS function-zero session has stopped");
    }
    ++next_sequence_;
}

void MillenniumDosVideoFunctionZeroSession::advance_success_postlude_prefix() {
    if (state_ != MillenniumDosVideoFunctionZeroState::mode_match_continuation_boundary
        || !outcome_
        || outcome_->endpoint != MillenniumDosVideoFunctionZeroEndpoint::mode_match_continuation) {
        throw std::runtime_error("Millennium DOS function-zero success continuation is not pending");
    }
    const auto& profile = driver_;
    const auto& returned = outcome_->verify_result;
    const auto cx = returned.cx;
    if (profile.kind == MillenniumDosVideoDriverKind::ega640) {
        const auto bounded_cx = cx > 4 ? std::uint16_t{4} : cx;
        postlude_prefix_outcome_ = MillenniumDosVideoFunctionZeroPostludeOutcome{
            MillenniumDosVideoFunctionZeroPostludeEndpoint::ega_pre_push,
            static_cast<std::uint16_t>(profile.function_zero_address + 0x35),
            returned.ax, 0x0072, bounded_cx, returned.dx, bounded_cx, std::nullopt,
            MillenniumDosVideoFunctionZeroLocalWrite{
                static_cast<std::uint16_t>(profile.function_zero_mode_match_branch_target),
                0x0192, 0},
            0};
    } else {
        const auto bounded_cx = cx > 8 ? std::uint16_t{8} : cx;
        const auto si = static_cast<std::uint16_t>(bounded_cx - 1);
        if (si == 0) {
            postlude_prefix_outcome_ = MillenniumDosVideoFunctionZeroPostludeOutcome{
                MillenniumDosVideoFunctionZeroPostludeEndpoint::mcga_single_count_branch,
                0x023d, returned.ax, returned.bx, bounded_cx, returned.dx, si, 0x0084,
                std::nullopt, 0};
        } else {
            postlude_prefix_outcome_ = MillenniumDosVideoFunctionZeroPostludeOutcome{
                MillenniumDosVideoFunctionZeroPostludeEndpoint::mcga_int92_request,
                0x021f, 0x0001, 0xfa00, bounded_cx, returned.dx, si, 0x0084,
                std::nullopt, 0x92};
        }
    }
    state_ = MillenniumDosVideoFunctionZeroState::mode_success_postlude_prefix_recorded;
}

void MillenniumDosVideoFunctionZeroSession::advance_ega_success_stack_prefix(
    const std::uint16_t ss, const std::uint16_t sp) {
    if (driver_.kind != MillenniumDosVideoDriverKind::ega640
        || state_ != MillenniumDosVideoFunctionZeroState::mode_success_postlude_prefix_recorded
        || !postlude_prefix_outcome_
        || postlude_prefix_outcome_->endpoint
            != MillenniumDosVideoFunctionZeroPostludeEndpoint::ega_pre_push) {
        throw std::runtime_error("Millennium DOS EGA stack continuation is not pending");
    }

    const auto pushed_value = postlude_prefix_outcome_->si;
    const auto sp_after = static_cast<std::uint16_t>(sp - 2U);
    const auto si = static_cast<std::uint16_t>(pushed_value - 1U);
    const bool zero_flag = si == 0;
    const auto& prior = *postlude_prefix_outcome_;
    if (zero_flag) {
        ega_stack_outcome_ = MillenniumDosVideoFunctionZeroEgaStackOutcome{
            MillenniumDosVideoFunctionZeroEgaStackEndpoint::ega_single_count_pop,
            0x022e, ss, sp, sp_after, pushed_value, si,
            prior.ax, prior.bx, prior.cx, prior.dx, true};
    } else {
        ega_stack_outcome_ = MillenniumDosVideoFunctionZeroEgaStackOutcome{
            MillenniumDosVideoFunctionZeroEgaStackEndpoint::ega_vga_out,
            0x0207, ss, sp, sp_after, pushed_value, si,
            0xff08, prior.bx, prior.cx, 0x03ce, false};
    }
    state_ = MillenniumDosVideoFunctionZeroState::mode_success_stack_prefix_recorded;
}

} // namespace eon
