#include "engine/millennium_dos_ninth_function_session.hpp"
#include <stdexcept>
namespace eon {
MillenniumDosNinthFunctionSession::MillenniumDosNinthFunctionSession(std::span<const std::uint8_t> executable)
    : trace_(parse_millennium_dos_game_flow(executable).ninth_function_key) {
    if (trace_.handler_address != 0x7339 || trace_.initialization_guard_address != 0xa19e
        || trace_.display_selector_call_address != 0xd0c9 || trace_.first_reset_runtime_byte_address != 0xda30
        || trace_.local_mode_address != 0x6e2f || trace_.second_reset_runtime_byte_address != 0xdad7
        || trace_.enabled_runtime_byte_address != 0xda39 || trace_.enabled_call_address != 0x7b47
        || trace_.limit_runtime_byte_address != 0xda06 || trace_.limit_value != 9
        || trace_.local_preflight_address != 0x731a || trace_.terminal_call_address != 0x14124)
        throw std::runtime_error("Unsupported Millennium DOS ninth-function profile");
}
void MillenniumDosNinthFunctionSession::enter_call(MillenniumDosNinthFunctionState state, std::uint16_t address, std::uint32_t target) { state_=state; call_address_=address; call_target_=target; }
MillenniumDosNinthFunctionBoundary MillenniumDosNinthFunctionSession::boundary() const {
    switch(state_) {
    case MillenniumDosNinthFunctionState::awaiting_guard: return {MillenniumDosNinthFunctionBoundaryKind::runtime_word,0x7339,trace_.initialization_guard_address,std::nullopt,0};
    case MillenniumDosNinthFunctionState::awaiting_enabled_byte: return {MillenniumDosNinthFunctionBoundaryKind::runtime_byte,0x7358,trace_.enabled_runtime_byte_address,std::nullopt,0};
    case MillenniumDosNinthFunctionState::awaiting_limit_byte: return {MillenniumDosNinthFunctionBoundaryKind::runtime_byte,0x7362,trace_.limit_runtime_byte_address,std::nullopt,loop_count_};
    case MillenniumDosNinthFunctionState::awaiting_conditional_byte: return {MillenniumDosNinthFunctionBoundaryKind::runtime_byte,0x7374,std::uint16_t{0xda09},std::nullopt,0};
    case MillenniumDosNinthFunctionState::returned_by_guard: return {MillenniumDosNinthFunctionBoundaryKind::local_return,0x7340,std::nullopt,std::nullopt,0};
    case MillenniumDosNinthFunctionState::terminal_handoff: return {MillenniumDosNinthFunctionBoundaryKind::jump_handoff,0x7381,std::nullopt,std::uint32_t{0x73cc},0};
    default: return {MillenniumDosNinthFunctionBoundaryKind::call_return,call_address_,std::nullopt,call_target_,loop_count_};
    }
}
void MillenniumDosNinthFunctionSession::observe_runtime_word(std::uint16_t instruction,std::uint16_t address,std::uint16_t value) {
    auto b=boundary(); if(b.kind!=MillenniumDosNinthFunctionBoundaryKind::runtime_word||b.instruction_address!=instruction||b.runtime_address!=address) throw std::runtime_error("Detached ninth-function word");
    if(value) state_=MillenniumDosNinthFunctionState::returned_by_guard; else enter_call(MillenniumDosNinthFunctionState::awaiting_display_return,0x7343,trace_.display_selector_call_address);
}
void MillenniumDosNinthFunctionSession::observe_runtime_byte(std::uint16_t instruction,std::uint16_t address,std::uint8_t value) {
    auto b=boundary(); if(b.kind!=MillenniumDosNinthFunctionBoundaryKind::runtime_byte||b.instruction_address!=instruction||b.runtime_address!=address) throw std::runtime_error("Detached ninth-function byte");
    if(state_==MillenniumDosNinthFunctionState::awaiting_enabled_byte) { if(value) enter_call(MillenniumDosNinthFunctionState::awaiting_enabled_return,0x735f,trace_.enabled_call_address); else state_=MillenniumDosNinthFunctionState::awaiting_limit_byte; return; }
    if(state_==MillenniumDosNinthFunctionState::awaiting_limit_byte) { if(value>=trace_.limit_value) { ++loop_count_; enter_call(MillenniumDosNinthFunctionState::awaiting_preflight_return,0x7369,trace_.local_preflight_address); } else { effects_.push_back({trace_.local_mode_address,trace_.local_mode_value,0}); state_=MillenniumDosNinthFunctionState::awaiting_conditional_byte; } return; }
    if(value==0) enter_call(MillenniumDosNinthFunctionState::awaiting_conditional_return,0x737b,0x7a9d); else enter_call(MillenniumDosNinthFunctionState::awaiting_terminal_return,0x737e,trace_.terminal_call_address);
}
void MillenniumDosNinthFunctionSession::observe_call_return(std::uint16_t call,std::uint16_t ret) {
    auto b=boundary(); if(b.kind!=MillenniumDosNinthFunctionBoundaryKind::call_return||b.instruction_address!=call||ret!=static_cast<std::uint16_t>(call+3U)) throw std::runtime_error("Detached ninth-function return");
    switch(state_) {
    case MillenniumDosNinthFunctionState::awaiting_display_return: effects_.push_back({trace_.first_reset_runtime_byte_address,std::nullopt,0}); effects_.push_back({trace_.local_mode_address,std::nullopt,1}); effects_.push_back({trace_.second_reset_runtime_byte_address,std::nullopt,0}); state_=MillenniumDosNinthFunctionState::awaiting_enabled_byte; break;
    case MillenniumDosNinthFunctionState::awaiting_enabled_return: case MillenniumDosNinthFunctionState::awaiting_preflight_return: state_=MillenniumDosNinthFunctionState::awaiting_limit_byte; break;
    case MillenniumDosNinthFunctionState::awaiting_conditional_return: enter_call(MillenniumDosNinthFunctionState::awaiting_terminal_return,0x737e,trace_.terminal_call_address); break;
    case MillenniumDosNinthFunctionState::awaiting_terminal_return: state_=MillenniumDosNinthFunctionState::terminal_handoff; break;
    default: throw std::runtime_error("Unsupported ninth-function call state");
    }
}
} // namespace eon
