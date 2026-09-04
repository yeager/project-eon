#pragma once
#include "data/millennium_dos_game_flow.hpp"
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
namespace eon {
enum class MillenniumDosFourthFunctionState { awaiting_guard, awaiting_first_return, awaiting_second_return, returned_by_guard, returned };
enum class MillenniumDosFourthFunctionBoundaryKind { runtime_word, call_return, local_return };
struct MillenniumDosFourthFunctionBoundary { MillenniumDosFourthFunctionBoundaryKind kind=MillenniumDosFourthFunctionBoundaryKind::runtime_word; std::uint16_t instruction_address=0; std::optional<std::uint16_t> runtime_address; std::optional<std::uint16_t> call_target; std::optional<std::uint16_t> known_ax; constexpr bool operator==(const MillenniumDosFourthFunctionBoundary&) const=default; };
struct MillenniumDosFourthFunctionByteEffect { std::uint16_t instruction_address=0; std::uint16_t runtime_address=0; std::uint8_t value=0; constexpr bool operator==(const MillenniumDosFourthFunctionByteEffect&) const=default; };
class MillenniumDosFourthFunctionSession {
public:
 explicit MillenniumDosFourthFunctionSession(std::span<const std::uint8_t> executable);
 [[nodiscard]] MillenniumDosFourthFunctionState state() const{return state_;}
 [[nodiscard]] MillenniumDosFourthFunctionBoundary boundary() const;
 [[nodiscard]] const std::vector<MillenniumDosFourthFunctionByteEffect>& effects() const{return effects_;}
 void observe_runtime_word(std::uint16_t instruction,std::uint16_t address,std::uint16_t value);
 void observe_call_return(std::uint16_t call,std::uint16_t ret);
private:
 MillenniumDosFourthFunctionKeyTrace trace_; MillenniumDosFourthFunctionState state_=MillenniumDosFourthFunctionState::awaiting_guard; std::vector<MillenniumDosFourthFunctionByteEffect> effects_;
};
} // namespace eon
