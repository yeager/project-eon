#include "engine/millennium_dos_sixth_function_session.hpp"

#include <array>
#include <stdexcept>

namespace eon {

MillenniumDosSixthFunctionSession::MillenniumDosSixthFunctionSession(
    const std::span<const std::uint8_t> game_executable)
    : trace_(parse_millennium_dos_game_flow(game_executable).sixth_function_key) {
    if (trace_.handler_address != 0x7415
        || trace_.initialization_guard_address != 0xa19e
        || trace_.display_selector_call_address != 0xd0c9
        || trace_.command_value != 0x0022 || trace_.first_call_address != 0x4d2c
        || trace_.second_call_address != 0xc980
        || trace_.saved_first_byte_address != 0x7412 || trace_.first_byte_address != 0x75a8
        || trace_.saved_second_byte_address != 0x740f || trace_.second_byte_address != 0x75ae
        || trace_.saved_word_address != 0x7410 || trace_.word_address != 0x75ac
        || trace_.callback_word_address != 0x75a6 || trace_.callback_word_value != 0x3207
        || trace_.wait_call_address != 0x09fa
        || trace_.restoration_handler_address != 0x7455
        || trace_.restoration_first_source_address != 0x740f
        || trace_.restoration_first_destination_address != 0x75ae
        || trace_.restoration_word_source_address != 0x7410
        || trace_.restoration_word_destination_address != 0x75ac
        || trace_.restoration_second_source_address != 0x7412
        || trace_.restoration_second_destination_address != 0x75a8
        || trace_.restoration_first_call_address != 0x0b0c
        || trace_.restoration_caller_address != 0x74c1
        || trace_.restoration_caller_call_address != 0x74c1
        || trace_.restoration_caller_target_address != 0xcc4e
        || trace_.restoration_caller_jump_address != 0x7455) {
        throw std::runtime_error("Unsupported Millennium DOS sixth-function profile");
    }
}

MillenniumDosSixthFunctionBoundary MillenniumDosSixthFunctionSession::boundary() const {
    MillenniumDosSixthFunctionBoundary result;
    switch (state_) {
    case MillenniumDosSixthFunctionState::awaiting_initialization_guard:
        result.kind = MillenniumDosSixthFunctionBoundaryKind::runtime_word;
        result.instruction_address = 0x7415;
        result.runtime_address = trace_.initialization_guard_address;
        break;
    case MillenniumDosSixthFunctionState::awaiting_first_byte:
        result.kind = MillenniumDosSixthFunctionBoundaryKind::runtime_byte;
        result.instruction_address = 0x742b;
        result.runtime_address = trace_.first_byte_address;
        break;
    case MillenniumDosSixthFunctionState::awaiting_second_byte:
        result.kind = MillenniumDosSixthFunctionBoundaryKind::runtime_byte;
        result.instruction_address = 0x7431;
        result.runtime_address = trace_.second_byte_address;
        break;
    case MillenniumDosSixthFunctionState::restoration_runtime_byte:
        result.kind = MillenniumDosSixthFunctionBoundaryKind::runtime_byte;
        result.instruction_address = 0x7483;
        result.runtime_address = 0x613a;
        break;
    case MillenniumDosSixthFunctionState::caller_helper_far_offset:
        result.kind = MillenniumDosSixthFunctionBoundaryKind::runtime_word;
        result.instruction_address = 0xcc73;
        result.runtime_address = 0x0112;
        break;
    case MillenniumDosSixthFunctionState::caller_helper_far_segment:
        result.kind = MillenniumDosSixthFunctionBoundaryKind::runtime_word;
        result.instruction_address = 0xcc73;
        result.runtime_address = 0x0114;
        break;
    case MillenniumDosSixthFunctionState::caller_helper_saved_byte:
        result.kind = MillenniumDosSixthFunctionBoundaryKind::runtime_byte;
        result.instruction_address = 0xcc80;
        result.runtime_address = 0xda05;
        break;
    case MillenniumDosSixthFunctionState::awaiting_word:
        result.kind = MillenniumDosSixthFunctionBoundaryKind::runtime_word;
        result.instruction_address = 0x7437;
        result.runtime_address = trace_.word_address;
        break;
    case MillenniumDosSixthFunctionState::awaiting_wait_bl:
        result.kind = MillenniumDosSixthFunctionBoundaryKind::register_bl;
        result.instruction_address = 0x7450;
        result.wait_iteration = wait_iteration_;
        break;
    case MillenniumDosSixthFunctionState::returned_by_guard:
        result.kind = MillenniumDosSixthFunctionBoundaryKind::local_return;
        result.instruction_address = 0x741c;
        break;
    case MillenniumDosSixthFunctionState::returned:
        result.kind = MillenniumDosSixthFunctionBoundaryKind::local_return;
        result.instruction_address = 0x7454;
        break;
    case MillenniumDosSixthFunctionState::restoration_returned:
        result.kind = MillenniumDosSixthFunctionBoundaryKind::local_return;
        result.instruction_address = 0x74aa;
        break;
    default:
        result.kind = MillenniumDosSixthFunctionBoundaryKind::call_return;
        result.instruction_address = call_address_;
        result.call_target = call_target_;
        result.known_ax = call_known_ax_;
        result.wait_iteration = wait_iteration_;
        break;
    }
    return result;
}

void MillenniumDosSixthFunctionSession::enter_call(
    const MillenniumDosSixthFunctionState state, const std::uint16_t address,
    const std::uint16_t target, const std::optional<std::uint16_t> known_ax) {
    state_ = state;
    call_address_ = address;
    call_target_ = target;
    call_known_ax_ = known_ax;
}

void MillenniumDosSixthFunctionSession::record_effect(
    const std::uint16_t address, const std::uint8_t width,
    const std::optional<std::uint16_t> previous, const std::uint16_t value) {
    effects_.push_back({address, width, previous, value});
}

void MillenniumDosSixthFunctionSession::observe_runtime_word(
    const std::uint16_t instruction_address, const std::uint16_t runtime_address,
    const std::uint16_t value) {
    const auto expected = boundary();
    if (expected.kind != MillenniumDosSixthFunctionBoundaryKind::runtime_word
        || expected.instruction_address != instruction_address
        || expected.runtime_address != runtime_address) {
        throw std::runtime_error("Millennium DOS sixth-function word observation is detached");
    }
    if (state_ == MillenniumDosSixthFunctionState::awaiting_initialization_guard) {
        if (value != 0) state_ = MillenniumDosSixthFunctionState::returned_by_guard;
        else enter_call(MillenniumDosSixthFunctionState::awaiting_display_call_return,
            0x741f, trace_.display_selector_call_address, 0);
        return;
    }
    if (state_ == MillenniumDosSixthFunctionState::caller_helper_far_offset) {
        caller_helper_far_offset_ = value;
        state_ = MillenniumDosSixthFunctionState::caller_helper_far_segment;
        return;
    }
    if (state_ == MillenniumDosSixthFunctionState::caller_helper_far_segment) {
        if (!caller_helper_far_offset_) {
            throw std::runtime_error("Millennium DOS F6 helper far pointer is detached");
        }
        constexpr std::uint16_t word_count = 0x0528;
        constexpr std::uint32_t byte_count = static_cast<std::uint32_t>(word_count) * 2U;
        if (static_cast<std::uint32_t>(*caller_helper_far_offset_) + byte_count
            > 0x10000U) {
            throw std::runtime_error("Millennium DOS F6 helper far clear would wrap");
        }
        caller_helper_far_segment_ = value;
        caller_helper_far_clear_effect_ = MillenniumDosSixthFunctionFarClearEffect{
            value, *caller_helper_far_offset_, word_count, 0};
        state_ = MillenniumDosSixthFunctionState::caller_helper_saved_byte;
        return;
    }
    if (state_ == MillenniumDosSixthFunctionState::awaiting_word) {
        record_effect(trace_.saved_word_address, 2, std::nullopt, value);
        record_effect(trace_.first_byte_address, 1, *first_byte_, trace_.first_byte_value);
        record_effect(trace_.second_byte_address, 1, *second_byte_, trace_.second_byte_value);
        record_effect(trace_.callback_word_address, 2, std::nullopt, trace_.callback_word_value);
        enter_call(MillenniumDosSixthFunctionState::awaiting_wait_call_return,
            0x744d, trace_.wait_call_address);
        return;
    }
    throw std::runtime_error("Unsupported Millennium DOS sixth-function word state");
}

void MillenniumDosSixthFunctionSession::observe_runtime_byte(
    const std::uint16_t instruction_address, const std::uint16_t runtime_address,
    const std::uint8_t value) {
    const auto expected = boundary();
    if (expected.kind != MillenniumDosSixthFunctionBoundaryKind::runtime_byte
        || expected.instruction_address != instruction_address
        || expected.runtime_address != runtime_address) {
        throw std::runtime_error("Millennium DOS sixth-function byte observation is detached");
    }
    if (state_ == MillenniumDosSixthFunctionState::restoration_runtime_byte) {
        enter_call(MillenniumDosSixthFunctionState::restoration_fifth_call_return,
            0x7487, 0x6178);
        return;
    }
    if (state_ == MillenniumDosSixthFunctionState::caller_helper_saved_byte) {
        caller_helper_saved_byte_ = value;
        caller_helper_state_clear_effect_ = MillenniumDosSixthFunctionStateClearEffect{
            0xda02, 0x0145, 0, 0xda05, value};
        record_effect(0xda05, 1, 0, value);
        for (const auto address : std::to_array<std::uint16_t>({
                 0xdb16, 0xda20, 0xda22, 0xda24, 0xda3b, 0xdb10})) {
            record_effect(address, 2, std::nullopt, 1);
        }
        record_effect(0xda26, 1, std::nullopt, 1);
        record_effect(0xda42, 1, std::nullopt, 0x80);
        record_effect(0xdb12, 1, std::nullopt, 9);
        enter_call(MillenniumDosSixthFunctionState::caller_helper_external_continuation,
            0xccba, 0x942c);
        return;
    }
    if (state_ == MillenniumDosSixthFunctionState::awaiting_first_byte) {
        first_byte_ = value;
        record_effect(trace_.saved_first_byte_address, 1, std::nullopt, value);
        state_ = MillenniumDosSixthFunctionState::awaiting_second_byte;
        return;
    }
    if (state_ == MillenniumDosSixthFunctionState::awaiting_second_byte) {
        second_byte_ = value;
        record_effect(trace_.saved_second_byte_address, 1, std::nullopt, value);
        state_ = MillenniumDosSixthFunctionState::awaiting_word;
        return;
    }
    throw std::runtime_error("Unsupported Millennium DOS sixth-function byte state");
}

void MillenniumDosSixthFunctionSession::observe_call_return(
    const std::uint16_t call_address, const std::uint16_t return_address) {
    const auto expected = boundary();
    if (expected.kind != MillenniumDosSixthFunctionBoundaryKind::call_return
        || expected.instruction_address != call_address
        || return_address != static_cast<std::uint16_t>(call_address + 3U)) {
        throw std::runtime_error("Millennium DOS sixth-function return is detached from its call");
    }
    switch (state_) {
    case MillenniumDosSixthFunctionState::awaiting_display_call_return:
        enter_call(MillenniumDosSixthFunctionState::awaiting_command_call_return,
            0x7425, trace_.first_call_address, trace_.command_value);
        return;
    case MillenniumDosSixthFunctionState::awaiting_command_call_return:
        enter_call(MillenniumDosSixthFunctionState::awaiting_second_call_return,
            0x7428, trace_.second_call_address);
        return;
    case MillenniumDosSixthFunctionState::awaiting_second_call_return:
        state_ = MillenniumDosSixthFunctionState::awaiting_first_byte;
        return;
    case MillenniumDosSixthFunctionState::awaiting_wait_call_return:
        state_ = MillenniumDosSixthFunctionState::awaiting_wait_bl;
        return;
    case MillenniumDosSixthFunctionState::restoration_caller_call_return: {
        std::optional<std::uint16_t> first;
        std::optional<std::uint16_t> word;
        std::optional<std::uint16_t> second;
        for (auto it = effects_.rbegin(); it != effects_.rend(); ++it) {
            if (!first && it->address == trace_.saved_first_byte_address) first = it->value;
            if (!word && it->address == trace_.saved_word_address) word = it->value;
            if (!second && it->address == trace_.saved_second_byte_address) second = it->value;
        }
        if (!first || !word || !second) {
            throw std::runtime_error(
                "Millennium DOS sixth-function restoration lacks saved state");
        }
        record_effect(trace_.restoration_first_destination_address, 1,
            std::nullopt, *second);
        record_effect(trace_.restoration_word_destination_address, 2,
            std::nullopt, *word);
        record_effect(trace_.restoration_second_destination_address, 1,
            std::nullopt, *first);
        enter_call(MillenniumDosSixthFunctionState::restoration_first_call_return,
            0x7467, trace_.restoration_first_call_address);
        return;
    }
    case MillenniumDosSixthFunctionState::caller_helper_first_call_return:
        enter_call(MillenniumDosSixthFunctionState::caller_helper_second_call_return,
            0xcc58, 0x4d36, 0x0028);
        return;
    case MillenniumDosSixthFunctionState::caller_helper_second_call_return:
        enter_call(MillenniumDosSixthFunctionState::caller_helper_third_call_return,
            0xcc5e, 0x0666, 0x00c1);
        return;
    case MillenniumDosSixthFunctionState::caller_helper_third_call_return:
        enter_call(MillenniumDosSixthFunctionState::caller_helper_fourth_call_return,
            0xcc64, 0x05f1);
        return;
    case MillenniumDosSixthFunctionState::caller_helper_fourth_call_return:
        record_effect(0xcbbe, 2, std::nullopt, 0x080f);
        record_effect(0xcbe1, 2, std::nullopt, 0x0000);
        state_ = MillenniumDosSixthFunctionState::caller_helper_far_offset;
        return;
    case MillenniumDosSixthFunctionState::restoration_first_call_return:
        enter_call(MillenniumDosSixthFunctionState::restoration_second_call_return,
            0x746e, 0x7b47);
        return;
    case MillenniumDosSixthFunctionState::restoration_second_call_return:
        enter_call(MillenniumDosSixthFunctionState::restoration_third_call_return,
            0x7471,0x0edb);
        return;
    case MillenniumDosSixthFunctionState::restoration_third_call_return:
        enter_call(MillenniumDosSixthFunctionState::restoration_fourth_call_return,
            0x7474,0x6baa);
        return;
    case MillenniumDosSixthFunctionState::restoration_fourth_call_return:
        record_effect(0x6132,2,std::nullopt,0x6092);
        enter_call(MillenniumDosSixthFunctionState::restoration_sixth_call_return,
            0x747d,0x6a85,0x6092);
        return;
    case MillenniumDosSixthFunctionState::restoration_sixth_call_return:
        state_=MillenniumDosSixthFunctionState::restoration_runtime_byte;
        return;
    case MillenniumDosSixthFunctionState::restoration_fifth_call_return:
        enter_call(MillenniumDosSixthFunctionState::restoration_seventh_call_return,
            0x748d, 0x6189);
        return;
    case MillenniumDosSixthFunctionState::restoration_seventh_call_return:
        record_effect(0x613a,1,std::nullopt,0);
        record_effect(0x6132,2,0x6092,0x5fe6);
        enter_call(MillenniumDosSixthFunctionState::restoration_eighth_call_return,
            0x749b,0x6a85,0x5fe6);
        return;
    case MillenniumDosSixthFunctionState::restoration_eighth_call_return:
        enter_call(MillenniumDosSixthFunctionState::restoration_ninth_call_return,
            0x749e,0x5bf5);
        return;
    case MillenniumDosSixthFunctionState::restoration_ninth_call_return:
        enter_call(MillenniumDosSixthFunctionState::restoration_tenth_call_return,
            0x74a1,0x0eea);
        return;
    case MillenniumDosSixthFunctionState::restoration_tenth_call_return:
        enter_call(MillenniumDosSixthFunctionState::restoration_eleventh_call_return,
            0x74a4,0x0b76);
        return;
    case MillenniumDosSixthFunctionState::restoration_eleventh_call_return:
        enter_call(MillenniumDosSixthFunctionState::restoration_twelfth_call_return,
            0x74a7,0x0bdf);
        return;
    case MillenniumDosSixthFunctionState::restoration_twelfth_call_return:
        state_=MillenniumDosSixthFunctionState::restoration_returned;
        return;
    default:
        throw std::runtime_error("Unsupported Millennium DOS sixth-function call state");
    }
}

void MillenniumDosSixthFunctionSession::begin_restoration() {
    if (state_ != MillenniumDosSixthFunctionState::returned) {
        throw std::runtime_error("Millennium DOS sixth-function restoration is detached");
    }
    enter_call(MillenniumDosSixthFunctionState::restoration_caller_call_return,
        trace_.restoration_caller_call_address,
        trace_.restoration_caller_target_address);
}

void MillenniumDosSixthFunctionSession::begin_restoration_caller_helper_prefix() {
    if (state_ != MillenniumDosSixthFunctionState::returned) {
        throw std::runtime_error("Millennium DOS F6 caller-helper prefix is detached");
    }
    enter_call(MillenniumDosSixthFunctionState::caller_helper_first_call_return,
        0xcc4e, 0x408a);
}

void MillenniumDosSixthFunctionSession::observe_bl(
    const std::uint16_t shift_address, const std::uint8_t value) {
    if (state_ != MillenniumDosSixthFunctionState::awaiting_wait_bl
        || shift_address != 0x7450) {
        throw std::runtime_error("Millennium DOS sixth-function BL observation is detached");
    }
    shifted_bl_values_.push_back(static_cast<std::uint8_t>(value >> 1U));
    if ((value & 1U) != 0) {
        ++wait_iteration_;
        enter_call(MillenniumDosSixthFunctionState::awaiting_wait_call_return,
            0x744d, trace_.wait_call_address);
    } else {
        state_ = MillenniumDosSixthFunctionState::returned;
    }
}

} // namespace eon
