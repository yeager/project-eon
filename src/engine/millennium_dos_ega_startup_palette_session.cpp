#include "engine/millennium_dos_ega_startup_palette_session.hpp"

#include <stdexcept>

namespace eon {

MillenniumDosEgaStartupPaletteSession::MillenniumDosEgaStartupPaletteSession(
    const std::span<const std::uint8_t> exact_game_executable,
    const MillenniumDosGameFunctionFourDispatchResult& function_four) {
    if (!function_four.accepted
        || function_four.kind != MillenniumDosVideoDriverKind::ega640
        || function_four.continuation_address != 0x0476) {
        throw std::runtime_error("EGA palette loop requires its exact function-$04 entry");
    }
    const auto callees = parse_millennium_dos_english_game_startup_callees(
        exact_game_executable);
    const auto followups = parse_millennium_dos_english_game_startup_followups(
        exact_game_executable, callees);
    if (followups.palette_return_address != 0x047c
        || followups.palette_initial_cx != writes_.size()
        || followups.bios_interrupt != 0x10
        || followups.bios_ax != 0x1000) {
        throw std::runtime_error("English EGA startup palette loop profile changed");
    }
    const auto flow = parse_millennium_dos_game_flow(exact_game_executable);
    writes_ = millennium_dos_startup_ega_palette_register_writes(flow);
}

std::optional<MillenniumDosEgaPaletteRegisterWrite>
MillenniumDosEgaStartupPaletteSession::pending_write() const {
    if (complete()) return std::nullopt;
    return writes_[next_ordinal_];
}

std::uint16_t MillenniumDosEgaStartupPaletteSession::next_instruction_address() const {
    return complete() ? 0x047c : 0x0476;
}

MillenniumDosEgaPaletteWriteResult
MillenniumDosEgaStartupPaletteSession::observe_applied_write(
    const MillenniumDosEgaPaletteWriteObservation& observation) {
    if (complete() || observation.sequence == 0 || observation.sequence <= last_sequence_
        || observation.interrupt_instruction_address != 0x0476
        || observation.interrupt_number != 0x10 || observation.ax != 0x1000) {
        return {false, "Palette write is detached from the exact EGA BIOS request", {},
            next_ordinal_, next_instruction_address(), complete()};
    }
    const auto expected = writes_[next_ordinal_];
    const auto expected_bx = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(expected.color_value) << 8U) | expected.register_index);
    if (observation.bx != expected_bx) {
        return {false, "Palette register/value differs from the hash-locked game table", {},
            next_ordinal_, next_instruction_address(), complete()};
    }
    last_sequence_ = observation.sequence;
    palette_registers_[expected.register_index] = expected.color_value;
    ++next_ordinal_;
    return {true, {}, expected, next_ordinal_, next_instruction_address(), complete()};
}

MillenniumDosEgaPaletteWriteResult
MillenniumDosEgaStartupPaletteSession::observe_write_return(
    const MillenniumDosEgaPaletteWriteObservation& observation) {
    if (complete() || observation.sequence == 0 || observation.sequence <= last_sequence_
        || observation.interrupt_instruction_address != 0x0476
        || observation.interrupt_number != 0x10 || observation.ax != 0x1000) {
        return {false, "Palette return is detached from the exact EGA BIOS request", {},
            next_ordinal_, next_instruction_address(), complete()};
    }
    const auto expected = writes_[next_ordinal_];
    const auto expected_bx = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(expected.color_value) << 8U) | expected.register_index);
    if (observation.bx != expected_bx) {
        return {false, "Palette register/value differs from the hash-locked game table", {},
            next_ordinal_, next_instruction_address(), complete()};
    }
    last_sequence_ = observation.sequence;
    ++next_ordinal_;
    return {true, {}, expected, next_ordinal_, next_instruction_address(), complete()};
}

} // namespace eon
