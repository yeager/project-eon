#include "engine/millennium_dos_second_function_session.hpp"
#include <stdexcept>

namespace eon {
MillenniumDosSecondFunctionSession::MillenniumDosSecondFunctionSession(std::span<const std::uint8_t> executable):trace_(parse_millennium_dos_game_flow(executable).second_function_key){
 if(trace_.handler_address!=0x71ca||trace_.availability_address!=0xda26||trace_.minimum_availability!=2||trace_.wait_call_address!=0x09fa||trace_.callback_slot_address!=0x6f98||trace_.callback_address!=0x7221||trace_.first_record_address!=0x1384||trace_.record_stride!=0x00c0||trace_.record_list_address!=0x6e99||trace_.list_mode_address!=0x6e98||trace_.list_mode_value!=1)throw std::runtime_error("Unsupported Millennium DOS second-function profile");
}
MillenniumDosSecondFunctionBoundary MillenniumDosSecondFunctionSession::boundary()const{
 switch(state_){
 case MillenniumDosSecondFunctionState::awaiting_availability:return{MillenniumDosSecondFunctionBoundaryKind::runtime_byte,0x71cc,trace_.availability_address,{},{},0};
 case MillenniumDosSecondFunctionState::awaiting_wait_return:return{MillenniumDosSecondFunctionBoundaryKind::call_return,0x71d6,{},trace_.wait_call_address,{},wait_iteration_};
 case MillenniumDosSecondFunctionState::awaiting_wait_bl:return{MillenniumDosSecondFunctionBoundaryKind::register_bl,0x71d9,{},{},{},wait_iteration_};
 case MillenniumDosSecondFunctionState::awaiting_first_setup_return:return{MillenniumDosSecondFunctionBoundaryKind::call_return,0x71ec,{},0x4d2c,0x18,0};
 case MillenniumDosSecondFunctionState::awaiting_second_setup_return:return{MillenniumDosSecondFunctionBoundaryKind::call_return,0x71f2,{},0x4d36,0x19,0};
 case MillenniumDosSecondFunctionState::awaiting_list_call_return:return{MillenniumDosSecondFunctionBoundaryKind::call_return,0x7215,{},0x72b5,{},0};
 case MillenniumDosSecondFunctionState::awaiting_terminal_call_return:return{MillenniumDosSecondFunctionBoundaryKind::call_return,0x721d,{},0x0b76,{},0};
 case MillenniumDosSecondFunctionState::returned_by_wait:return{MillenniumDosSecondFunctionBoundaryKind::local_return,0x71dd,{},{},{},0};
 case MillenniumDosSecondFunctionState::returned:return{MillenniumDosSecondFunctionBoundaryKind::local_return,0x7220,{},{},{},0};}
 throw std::runtime_error("Unsupported Millennium DOS second-function state");
}
void MillenniumDosSecondFunctionSession::observe_runtime_byte(std::uint16_t i,std::uint16_t a,std::uint8_t v){
 auto b=boundary();if(b.kind!=MillenniumDosSecondFunctionBoundaryKind::runtime_byte||b.instruction_address!=i||b.runtime_address!=a)throw std::runtime_error("Detached Millennium DOS second-function byte");
 if(v<trace_.minimum_availability){state_=MillenniumDosSecondFunctionState::awaiting_wait_return;return;}
 availability_=v;effects_.push_back({0x71e1,trace_.callback_slot_address,2,trace_.callback_address});effects_.push_back({0x71e4,trace_.list_mode_address,1,trace_.list_mode_value});state_=MillenniumDosSecondFunctionState::awaiting_first_setup_return;
}
void MillenniumDosSecondFunctionSession::observe_call_return(std::uint16_t c,std::uint16_t r){auto b=boundary();if(b.kind!=MillenniumDosSecondFunctionBoundaryKind::call_return||b.instruction_address!=c||r!=std::uint16_t(c+3))throw std::runtime_error("Detached Millennium DOS second-function return");switch(state_){case MillenniumDosSecondFunctionState::awaiting_wait_return:state_=MillenniumDosSecondFunctionState::awaiting_wait_bl;break;case MillenniumDosSecondFunctionState::awaiting_first_setup_return:state_=MillenniumDosSecondFunctionState::awaiting_second_setup_return;break;case MillenniumDosSecondFunctionState::awaiting_second_setup_return:effects_.push_back({0x71ff,0x6e95,1,static_cast<std::uint8_t>(availability_-1)});for(std::uint16_t n=0;n<std::uint16_t(availability_-1);++n)effects_.push_back({0x7208,static_cast<std::uint16_t>(trace_.record_list_address+n*2),2,static_cast<std::uint16_t>(trace_.first_record_address+n*trace_.record_stride)});effects_.push_back({0x7210,0x6e93,1,0xff});state_=MillenniumDosSecondFunctionState::awaiting_list_call_return;break;case MillenniumDosSecondFunctionState::awaiting_list_call_return:effects_.push_back({0x7218,0xda1e,1,8});state_=MillenniumDosSecondFunctionState::awaiting_terminal_call_return;break;case MillenniumDosSecondFunctionState::awaiting_terminal_call_return:state_=MillenniumDosSecondFunctionState::returned;break;default:throw std::runtime_error("Detached Millennium DOS second-function return");}}
void MillenniumDosSecondFunctionSession::observe_bl(std::uint16_t i,std::uint8_t v){if(state_!=MillenniumDosSecondFunctionState::awaiting_wait_bl||i!=0x71d9)throw std::runtime_error("Detached Millennium DOS second-function BL");if(v&1){++wait_iteration_;state_=MillenniumDosSecondFunctionState::awaiting_wait_return;}else state_=MillenniumDosSecondFunctionState::returned_by_wait;}
}
