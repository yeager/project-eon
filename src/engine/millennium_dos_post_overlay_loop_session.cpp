#include "engine/millennium_dos_post_overlay_loop_session.hpp"

#include <stdexcept>

namespace eon {
namespace {
constexpr std::uint16_t private_interrupt_site = 0x0129;
constexpr std::uint16_t palette_interrupt_site = 0x0476;
}

MillenniumDosPostOverlayLoopSession::MillenniumDosPostOverlayLoopSession(
    const std::span<const std::uint8_t> game_executable,
    const std::uint8_t selected_mode_byte)
    : game_executable_(game_executable),
      loop_(parse_millennium_dos_post_overlay_adapter_loop(game_executable)),
      dispatch_(parse_millennium_dos_post_overlay_dispatch_prefix(game_executable)),
      selected_mode_byte_(selected_mode_byte) {
    const auto callees = parse_millennium_dos_english_game_startup_callees(game_executable_);
    const auto followups = parse_millennium_dos_english_game_startup_followups(
        game_executable_, callees);
    const auto private_wrapper = parse_millennium_dos_private_int91_wrapper(game_executable_);
    if (loop_.entry_address != 0xd39d || loop_.following_dispatch_address != dispatch_.entry_address
        || callees.equal_private_target_address != private_wrapper.entry_address
        || callees.other_private_target_address != private_wrapper.entry_address
        || private_wrapper.private_interrupt_site != private_interrupt_site
        || private_wrapper.private_interrupt != 0x91
        || followups.equal_return_address != 0x0455
        || followups.palette_entry_address != 0x0466
        || followups.bios_interrupt != 0x10
        || palette_interrupt_site >= followups.palette_return_address) {
        throw std::runtime_error("Unsupported Millennium DOS post-overlay loop connection");
    }
}

MillenniumDosPostOverlayLoopBoundary MillenniumDosPostOverlayLoopSession::boundary() const {
    switch (state_) {
    case MillenniumDosPostOverlayLoopState::awaiting_private_interrupt_return:
        return {MillenniumDosPostOverlayLoopBoundaryKind::private_interrupt,
            private_interrupt_site, std::nullopt, std::nullopt, std::uint8_t{0x91}, 0};
    case MillenniumDosPostOverlayLoopState::palette_bios_interrupt_boundary:
        return {MillenniumDosPostOverlayLoopBoundaryKind::bios_interrupt,
            palette_interrupt_site, std::nullopt, std::nullopt, std::uint8_t{0x10}, 0};
    case MillenniumDosPostOverlayLoopState::awaiting_call_return:
        return {MillenniumDosPostOverlayLoopBoundaryKind::call_return,
            loop_.call_addresses[call_ordinal_], loop_.call_targets[call_ordinal_],
            std::nullopt, std::nullopt, call_ordinal_};
    case MillenniumDosPostOverlayLoopState::awaiting_first_al:
        return {MillenniumDosPostOverlayLoopBoundaryKind::register_value,
            loop_.first_al_test_address, std::nullopt, std::nullopt, std::nullopt, 0};
    case MillenniumDosPostOverlayLoopState::awaiting_toggle_source_byte:
        return {MillenniumDosPostOverlayLoopBoundaryKind::runtime_byte,
            loop_.native_byte_load_address, std::nullopt, loop_.native_byte_address,
            std::nullopt, 0};
    case MillenniumDosPostOverlayLoopState::awaiting_action_al:
        return {MillenniumDosPostOverlayLoopBoundaryKind::register_value,
            loop_.loop_al_test_address, std::nullopt, std::nullopt, std::nullopt,
            action_poll_count_};
    case MillenniumDosPostOverlayLoopState::awaiting_dispatch_guard_byte:
        return {MillenniumDosPostOverlayLoopBoundaryKind::runtime_byte,
            dispatch_.guard_load_address, std::nullopt, dispatch_.guard_byte_address,
            std::nullopt, action_poll_count_};
    case MillenniumDosPostOverlayLoopState::dispatch_call_boundary:
        return {MillenniumDosPostOverlayLoopBoundaryKind::dispatch_call,
            *dispatch_call_address_, dispatch_call_target_, std::nullopt, std::nullopt,
            function_key_index_.value_or(0)};
    }
    throw std::runtime_error("Unsupported Millennium DOS post-overlay loop state");
}

void MillenniumDosPostOverlayLoopSession::enter_call(const std::size_t ordinal) {
    if (ordinal >= loop_.call_addresses.size()) {
        throw std::runtime_error("Millennium DOS post-overlay call ordinal is outside its loop");
    }
    call_ordinal_ = ordinal;
    state_ = MillenniumDosPostOverlayLoopState::awaiting_call_return;
}

void MillenniumDosPostOverlayLoopSession::observe_private_interrupt_return(
    const std::uint16_t interrupt_address, const std::uint16_t ax) {
    if (state_ != MillenniumDosPostOverlayLoopState::awaiting_private_interrupt_return
        || interrupt_address != private_interrupt_site) {
        throw std::runtime_error("Millennium DOS post-overlay private return is detached");
    }
    observed_private_interrupt_ax_ = ax;
    if (selected_mode_byte_ != 1) {
        // The non-equal callee enters the separately verified palette loop.
        // It cannot reach $d39d until all BIOS behavior is observed.
        state_ = MillenniumDosPostOverlayLoopState::palette_bios_interrupt_boundary;
        return;
    }
    // Selector one executes the verified $044e prefix before returning to the
    // caller. Its sole write is idempotent with the already observed mode.
    runtime_effects_.push_back({0xda05, selected_mode_byte_, 1});
    enter_call(0);
}

void MillenniumDosPostOverlayLoopSession::observe_call_return(
    const std::uint16_t call_address, const std::uint16_t return_address) {
    if (state_ != MillenniumDosPostOverlayLoopState::awaiting_call_return
        || call_address != loop_.call_addresses[call_ordinal_]
        || return_address != static_cast<std::uint16_t>(call_address + 3U)) {
        throw std::runtime_error("Millennium DOS post-overlay return is detached from its call");
    }
    ++completed_call_return_count_;
    if (call_ordinal_ == 6) {
        state_ = MillenniumDosPostOverlayLoopState::awaiting_first_al;
    } else if (call_ordinal_ == 14) {
        ++action_poll_count_;
        state_ = MillenniumDosPostOverlayLoopState::awaiting_action_al;
    } else {
        enter_call(call_ordinal_ + 1);
    }
}

void MillenniumDosPostOverlayLoopSession::observe_al(
    const std::uint16_t test_address, const std::uint8_t value) {
    if (state_ == MillenniumDosPostOverlayLoopState::awaiting_first_al) {
        if (test_address != loop_.first_al_test_address) {
            throw std::runtime_error("Millennium DOS first AL observation is detached from its test");
        }
        if (value == 0) {
            state_ = MillenniumDosPostOverlayLoopState::awaiting_toggle_source_byte;
        } else {
            enter_call(7);
        }
        return;
    }
    if (state_ != MillenniumDosPostOverlayLoopState::awaiting_action_al
        || test_address != loop_.loop_al_test_address) {
        throw std::runtime_error("Millennium DOS action observation is detached from its test");
    }
    observed_action_ = value;
    function_key_index_.reset();
    if (value == 0) {
        enter_call(11);
    } else if (value == dispatch_.first_action_value) {
        finish_dispatch(dispatch_.first_action_call_address,
            dispatch_.first_action_call_target);
    } else {
        state_ = MillenniumDosPostOverlayLoopState::awaiting_dispatch_guard_byte;
    }
}

void MillenniumDosPostOverlayLoopSession::observe_runtime_byte(
    const std::uint16_t load_address, const std::uint16_t runtime_address,
    const std::uint8_t value) {
    if (state_ == MillenniumDosPostOverlayLoopState::awaiting_toggle_source_byte) {
        if (load_address != loop_.native_byte_load_address
            || runtime_address != loop_.native_byte_address) {
            throw std::runtime_error("Millennium DOS toggle observation is detached from its load");
        }
        runtime_effects_.push_back({runtime_address, value,
            static_cast<std::uint8_t>(value ^ loop_.native_byte_xor_literal)});
        enter_call(7);
        return;
    }
    if (state_ != MillenniumDosPostOverlayLoopState::awaiting_dispatch_guard_byte
        || load_address != dispatch_.guard_load_address
        || runtime_address != dispatch_.guard_byte_address || !observed_action_) {
        throw std::runtime_error("Millennium DOS dispatch guard is detached from its load");
    }
    if (value != 0) {
        enter_call(11);
        return;
    }
    if (*observed_action_ == dispatch_.second_action_value) {
        finish_dispatch(dispatch_.second_action_call_address,
            dispatch_.second_action_call_target);
        return;
    }
    if (*observed_action_ >= dispatch_.action_base_value) {
        const auto index = static_cast<std::size_t>(
            *observed_action_ - dispatch_.action_base_value);
        if (index < dispatch_.action_limit_value) {
            finish_dispatch(dispatch_.scaled_call_address,
                dispatch_.scaled_call_target, index);
            return;
        }
    }
    // The original out-of-range path returns to the four-call poll tail.
    enter_call(11);
}

void MillenniumDosPostOverlayLoopSession::finish_dispatch(
    const std::uint16_t call_address, const std::uint16_t call_target,
    const std::optional<std::size_t> function_key_index) {
    dispatch_call_address_ = call_address;
    dispatch_call_target_ = call_target;
    function_key_index_ = function_key_index;
    state_ = MillenniumDosPostOverlayLoopState::dispatch_call_boundary;
}

} // namespace eon
