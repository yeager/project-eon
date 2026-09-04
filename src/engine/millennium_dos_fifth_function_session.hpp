#pragma once
#include "data/millennium_dos_game_flow.hpp"
#include <cstdint>
#include <optional>
#include <span>
namespace eon {
enum class MillenniumDosFifthFunctionState { first_call, second_call, third_call, fourth_call, returned };
enum class MillenniumDosFifthFunctionBoundaryKind { call_return, local_return };
struct MillenniumDosFifthFunctionBoundary { MillenniumDosFifthFunctionBoundaryKind kind=MillenniumDosFifthFunctionBoundaryKind::call_return; std::uint16_t instruction_address=0; std::optional<std::uint16_t> call_target; std::optional<std::uint16_t> known_ax; constexpr bool operator==(const MillenniumDosFifthFunctionBoundary&)const=default; };
class MillenniumDosFifthFunctionSession { public: explicit MillenniumDosFifthFunctionSession(std::span<const std::uint8_t>); [[nodiscard]] MillenniumDosFifthFunctionState state()const{return state_;} [[nodiscard]] MillenniumDosFifthFunctionBoundary boundary()const; void observe_call_return(std::uint16_t,std::uint16_t); private: MillenniumDosFifthFunctionKeyTrace trace_; MillenniumDosFifthFunctionState state_=MillenniumDosFifthFunctionState::first_call; };
} // namespace eon
