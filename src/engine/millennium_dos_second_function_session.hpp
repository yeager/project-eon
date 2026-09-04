#pragma once

#include "data/millennium_dos_game_flow.hpp"
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace eon {
enum class MillenniumDosSecondFunctionState { awaiting_availability, awaiting_wait_return, awaiting_wait_bl, awaiting_first_setup_return, awaiting_second_setup_return, awaiting_list_call_return, awaiting_terminal_call_return, returned_by_wait, returned };
enum class MillenniumDosSecondFunctionBoundaryKind { runtime_byte, call_return, register_bl, local_return };
struct MillenniumDosSecondFunctionBoundary { MillenniumDosSecondFunctionBoundaryKind kind; std::uint16_t instruction_address; std::optional<std::uint16_t> runtime_address; std::optional<std::uint16_t> call_target; std::optional<std::uint16_t> known_ax; std::size_t wait_iteration=0; constexpr bool operator==(const MillenniumDosSecondFunctionBoundary&) const=default; };
struct MillenniumDosSecondFunctionEffect { std::uint16_t instruction_address; std::uint16_t runtime_address; std::uint8_t width; std::uint16_t value; constexpr bool operator==(const MillenniumDosSecondFunctionEffect&) const=default; };
class MillenniumDosSecondFunctionSession {
public:
 explicit MillenniumDosSecondFunctionSession(std::span<const std::uint8_t> executable);
 [[nodiscard]] MillenniumDosSecondFunctionState state()const{return state_;}
 [[nodiscard]] MillenniumDosSecondFunctionBoundary boundary()const;
 [[nodiscard]] const std::vector<MillenniumDosSecondFunctionEffect>& effects()const{return effects_;}
 void observe_runtime_byte(std::uint16_t instruction,std::uint16_t address,std::uint8_t value);
 void observe_call_return(std::uint16_t call,std::uint16_t returned_to);
 void observe_bl(std::uint16_t instruction,std::uint8_t value);
private:
 MillenniumDosSecondFunctionKeyTrace trace_;
 MillenniumDosSecondFunctionState state_=MillenniumDosSecondFunctionState::awaiting_availability;
 std::size_t wait_iteration_=0;
 std::uint8_t availability_=0;
 std::vector<MillenniumDosSecondFunctionEffect> effects_;
};
}
