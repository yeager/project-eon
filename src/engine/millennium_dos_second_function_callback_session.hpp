#pragma once
#include "data/millennium_dos_game_flow.hpp"
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
namespace eon {
enum class MillenniumDosSecondFunctionCallbackState { awaiting_selection_byte, external_reset_jump, awaiting_record_pointer, awaiting_first_call_return, awaiting_second_call_return, awaiting_wait_return, awaiting_wait_bl, awaiting_third_call_return, awaiting_fourth_call_return, external_tail_jump };
enum class MillenniumDosSecondFunctionCallbackBoundaryKind { runtime_byte, runtime_word, call_return, register_bl, external_jump };
struct MillenniumDosSecondFunctionCallbackBoundary { MillenniumDosSecondFunctionCallbackBoundaryKind kind; std::uint16_t instruction_address; std::optional<std::uint16_t> runtime_address; std::optional<std::uint16_t> target; std::size_t wait_iteration=0; constexpr bool operator==(const MillenniumDosSecondFunctionCallbackBoundary&)const=default; };
struct MillenniumDosSecondFunctionCallbackEffect { std::uint16_t instruction_address;std::uint16_t runtime_address;std::uint8_t width;std::uint16_t value;constexpr bool operator==(const MillenniumDosSecondFunctionCallbackEffect&)const=default;};
class MillenniumDosSecondFunctionCallbackSession {
public:
 explicit MillenniumDosSecondFunctionCallbackSession(std::span<const std::uint8_t> executable);
 [[nodiscard]] auto state()const{return state_;} [[nodiscard]] MillenniumDosSecondFunctionCallbackBoundary boundary()const; [[nodiscard]] const auto& effects()const{return effects_;}
 void observe_runtime_byte(std::uint16_t,std::uint16_t,std::uint8_t);void observe_runtime_word(std::uint16_t,std::uint16_t,std::uint16_t);void observe_call_return(std::uint16_t,std::uint16_t);void observe_bl(std::uint16_t,std::uint8_t);
private: MillenniumDosSecondFunctionKeyTrace trace_;MillenniumDosSecondFunctionCallbackState state_=MillenniumDosSecondFunctionCallbackState::awaiting_selection_byte;std::size_t wait_iteration_=0;std::uint16_t pointer_cell_=0;std::vector<MillenniumDosSecondFunctionCallbackEffect> effects_;
};
}
