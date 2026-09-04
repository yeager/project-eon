#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
namespace eon {
enum class MillenniumDosNinthHandoffState { first_call,second_call,mode_byte,wait_call,wait_zero_flag,wait_bl,short_call,short_return,guard_word,optional_call,setup_first_call,setup_second_call,cx_word,setup_third_call,setup_fourth_call,long_return };
enum class MillenniumDosNinthHandoffBoundaryKind { call_return,runtime_byte,runtime_word,zero_flag,register_bl,local_return };
struct MillenniumDosNinthHandoffBoundary { MillenniumDosNinthHandoffBoundaryKind kind;std::uint16_t instruction_address;std::optional<std::uint16_t> runtime_address;std::optional<std::uint16_t> call_target;std::size_t loop_iteration=0;constexpr bool operator==(const MillenniumDosNinthHandoffBoundary&)const=default;};
struct MillenniumDosNinthHandoffEffect {std::uint16_t instruction_address;std::uint16_t runtime_address;std::uint8_t value;constexpr bool operator==(const MillenniumDosNinthHandoffEffect&)const=default;};
class MillenniumDosNinthFunctionHandoffSession { public: explicit MillenniumDosNinthFunctionHandoffSession(std::span<const std::uint8_t>);[[nodiscard]] auto state()const{return state_;}[[nodiscard]] MillenniumDosNinthHandoffBoundary boundary()const;[[nodiscard]]const auto& effects()const{return effects_;}void observe_call_return(std::uint16_t,std::uint16_t);void observe_runtime_byte(std::uint16_t,std::uint16_t,std::uint8_t);void observe_runtime_word(std::uint16_t,std::uint16_t,std::uint16_t);void observe_zero_flag(std::uint16_t,bool);void observe_bl(std::uint16_t,std::uint8_t);private:MillenniumDosNinthHandoffState state_=MillenniumDosNinthHandoffState::first_call;std::size_t loop_=0;std::vector<MillenniumDosNinthHandoffEffect> effects_;};
}
