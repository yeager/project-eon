#include "engine/millennium_dos_seventh_function_session.hpp"

#include <array>
#include <stdexcept>

namespace eon {
namespace {
struct CallSite { std::uint16_t address; std::uint16_t target; };
constexpr std::array<CallSite, 18> calls{{
    {0x752b, 0x4d2c}, {0x7531, 0x073c}, {0x7537, 0x0666},
    {0x7547, 0x0666}, {0x754d, 0x06e2}, {0x7555, 0x05ce},
    {0x755b, 0x06dc}, {0x755e, 0x05ce}, {0x7566, 0x06dc},
    {0x7569, 0x05ce}, {0x756f, 0x06dc}, {0x7572, 0x05ce},
    {0x757b, 0x06dc}, {0x7580, 0x077e}, {0x7588, 0x070a},
    {0x758d, 0x077e}, {0x7590, 0x0b9d}, {0x7593, 0x4bf7},
}};
}

MillenniumDosSeventhFunctionSession::MillenniumDosSeventhFunctionSession(
    const std::span<const std::uint8_t> game_executable)
    : game_executable_(game_executable),
      trace_(parse_millennium_dos_game_flow(game_executable).seventh_function_key) {
    if (trace_.handler_address != 0x7521 || trace_.initialization_guard_address != 0xa19e
        || trace_.first_call_address != calls[0].target
        || trace_.first_command_call_address != calls[1].target
        || trace_.second_command_call_address != calls[2].target
        || trace_.helper_a_address != 0x06dc || trace_.helper_b_address != 0x05ce
        || trace_.helper_c_address != 0x077e
        || trace_.terminal_call_address != calls.back().target) {
        throw std::runtime_error("Unsupported Millennium DOS seventh-function profile");
    }
}

MillenniumDosSeventhFunctionBoundary MillenniumDosSeventhFunctionSession::boundary() const {
    const auto value_boundary = [](const MillenniumDosSeventhFunctionBoundaryKind kind,
                                    const std::uint16_t instruction,
                                    const std::optional<std::uint16_t> runtime = std::nullopt) {
        MillenniumDosSeventhFunctionBoundary result;
        result.kind = kind;
        result.instruction_address = instruction;
        result.runtime_address = runtime;
        return result;
    };
    if (step_ == terminal_step) {
        return value_boundary(MillenniumDosSeventhFunctionBoundaryKind::local_return,
            returned_by_guard_ ? std::uint16_t{0x7528} : std::uint16_t{0x7596});
    }
    switch (step_) {
    case 0: return value_boundary(MillenniumDosSeventhFunctionBoundaryKind::runtime_word,
        0x7521, trace_.initialization_guard_address);
    case 4: return value_boundary(MillenniumDosSeventhFunctionBoundaryKind::runtime_byte,
        0x753a, trace_.first_runtime_word_address);
    case 5: return value_boundary(MillenniumDosSeventhFunctionBoundaryKind::runtime_word,
        0x7542, 0x05ca);
    case 7: return value_boundary(MillenniumDosSeventhFunctionBoundaryKind::runtime_word,
        0x754a, trace_.second_runtime_word_address);
    case 9: return value_boundary(MillenniumDosSeventhFunctionBoundaryKind::returned_register_bx,
        0x7550, 0x05ca);
    case 11: return value_boundary(MillenniumDosSeventhFunctionBoundaryKind::runtime_word,
        0x7558, trace_.third_runtime_word_address);
    case 14: return value_boundary(MillenniumDosSeventhFunctionBoundaryKind::runtime_byte,
        0x7561, trace_.fourth_runtime_word_address);
    case 17: return value_boundary(MillenniumDosSeventhFunctionBoundaryKind::runtime_word,
        0x756c, trace_.fifth_runtime_word_address);
    case 20: return value_boundary(MillenniumDosSeventhFunctionBoundaryKind::runtime_word,
        0x7575, trace_.sixth_runtime_word_address);
    default: break;
    }
    static constexpr std::array<std::size_t, 18> call_steps{{
        1,2,3,6,8,10,12,13,15,16,18,19,21,22,23,24,25,26}};
    for (std::size_t index = 0; index < call_steps.size(); ++index) {
        if (step_ != call_steps[index]) continue;
        MillenniumDosSeventhFunctionBoundary result{
            MillenniumDosSeventhFunctionBoundaryKind::call_return,
            calls[index].address, std::nullopt, calls[index].target,
            std::nullopt, std::nullopt, std::nullopt, index};
        switch (index) {
        case 0: result.known_al = trace_.initial_al_value; break;
        case 1: result.known_ax = trace_.first_command_value; break;
        case 2: result.known_ax = trace_.second_command_value; break;
        case 3:
            result.known_ax = static_cast<std::uint16_t>(*da17_ + 0x01a2U);
            result.known_bx = saved_05ca_;
            break;
        case 4: result.known_ax = da18_; break;
        case 6: result.known_ax = da27_; break;
        case 8: result.known_ax = da26_; break;
        case 10: result.known_ax = da35_; break;
        case 12: result.known_ax = static_cast<std::uint8_t>(*da37_); break;
        case 13: result.known_al = 0x2e; break;
        case 14: result.known_ax = static_cast<std::uint8_t>(*da37_ >> 8U); break;
        case 15: result.known_al = static_cast<std::uint8_t>(trace_.literal_al_value); break;
        default: break;
        }
        return result;
    }
    throw std::runtime_error("Unsupported Millennium DOS seventh-function state");
}

void MillenniumDosSeventhFunctionSession::require_boundary(
    const MillenniumDosSeventhFunctionBoundaryKind kind,
    const std::uint16_t instruction_address,
    const std::optional<std::uint16_t> runtime_address) const {
    const auto expected = boundary();
    if (expected.kind != kind || expected.instruction_address != instruction_address
        || (runtime_address && expected.runtime_address != runtime_address)) {
        throw std::runtime_error("Millennium DOS seventh-function observation is detached");
    }
}

void MillenniumDosSeventhFunctionSession::observe_runtime_word(
    const std::uint16_t instruction_address, const std::uint16_t runtime_address,
    const std::uint16_t value) {
    require_boundary(MillenniumDosSeventhFunctionBoundaryKind::runtime_word,
        instruction_address, runtime_address);
    switch (step_) {
    case 0:
        if (value != 0) { returned_by_guard_ = true; step_ = terminal_step; }
        else step_ = 1;
        return;
    case 5: saved_05ca_ = value; step_ = 6; return;
    case 7: da18_ = value; step_ = 8; return;
    case 11: da27_ = value; step_ = 12; return;
    case 17: da35_ = value; step_ = 18; return;
    case 20: da37_ = value; step_ = 21; return;
    default: throw std::runtime_error("Unsupported Millennium DOS seventh-function word state");
    }
}

void MillenniumDosSeventhFunctionSession::observe_runtime_byte(
    const std::uint16_t instruction_address, const std::uint16_t runtime_address,
    const std::uint8_t value) {
    require_boundary(MillenniumDosSeventhFunctionBoundaryKind::runtime_byte,
        instruction_address, runtime_address);
    if (step_ == 4) { da17_ = value; step_ = 5; return; }
    if (step_ == 14) { da26_ = value; step_ = 15; return; }
    throw std::runtime_error("Unsupported Millennium DOS seventh-function byte state");
}

void MillenniumDosSeventhFunctionSession::observe_call_return(
    const std::uint16_t call_address, const std::uint16_t return_address) {
    require_boundary(MillenniumDosSeventhFunctionBoundaryKind::call_return, call_address);
    if (return_address != static_cast<std::uint16_t>(call_address + 3U)) {
        throw std::runtime_error("Millennium DOS seventh-function return address is detached");
    }
    ++call_ordinal_;
    ++step_;
}

void MillenniumDosSeventhFunctionSession::observe_returned_bx(
    const std::uint16_t store_address, const std::uint16_t value) {
    require_boundary(MillenniumDosSeventhFunctionBoundaryKind::returned_register_bx,
        store_address, 0x05ca);
    runtime_effects_.push_back({0x05ca, *saved_05ca_, value});
    ++step_;
}

} // namespace eon
