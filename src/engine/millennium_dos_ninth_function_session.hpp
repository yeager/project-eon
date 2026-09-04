#pragma once
#include "data/millennium_dos_game_flow.hpp"
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
namespace eon {
enum class MillenniumDosNinthFunctionState { awaiting_guard, awaiting_display_return,
    awaiting_enabled_byte, awaiting_enabled_return, awaiting_limit_byte,
    awaiting_preflight_return, awaiting_conditional_byte, awaiting_conditional_return,
    awaiting_terminal_return, returned_by_guard, terminal_handoff };
enum class MillenniumDosNinthFunctionBoundaryKind { runtime_word, runtime_byte, call_return, local_return, jump_handoff };
struct MillenniumDosNinthFunctionBoundary {
    MillenniumDosNinthFunctionBoundaryKind kind = MillenniumDosNinthFunctionBoundaryKind::runtime_word;
    std::uint16_t instruction_address = 0;
    std::optional<std::uint16_t> runtime_address;
    std::optional<std::uint32_t> call_target;
    std::size_t loop_iteration = 0;
    constexpr bool operator==(const MillenniumDosNinthFunctionBoundary&) const = default;
};
struct MillenniumDosNinthFunctionByteEffect { std::uint16_t address = 0; std::optional<std::uint8_t> previous; std::uint8_t value = 0; constexpr bool operator==(const MillenniumDosNinthFunctionByteEffect&) const = default; };
class MillenniumDosNinthFunctionSession {
public:
    explicit MillenniumDosNinthFunctionSession(std::span<const std::uint8_t> executable);
    [[nodiscard]] MillenniumDosNinthFunctionState state() const { return state_; }
    [[nodiscard]] MillenniumDosNinthFunctionBoundary boundary() const;
    [[nodiscard]] const std::vector<MillenniumDosNinthFunctionByteEffect>& effects() const { return effects_; }
    [[nodiscard]] std::size_t loop_count() const { return loop_count_; }
    void observe_runtime_word(std::uint16_t instruction, std::uint16_t address, std::uint16_t value);
    void observe_runtime_byte(std::uint16_t instruction, std::uint16_t address, std::uint8_t value);
    void observe_call_return(std::uint16_t call_address, std::uint16_t return_address);
private:
    void enter_call(MillenniumDosNinthFunctionState, std::uint16_t, std::uint32_t);
    MillenniumDosNinthFunctionKeyTrace trace_;
    MillenniumDosNinthFunctionState state_ = MillenniumDosNinthFunctionState::awaiting_guard;
    std::uint16_t call_address_ = 0; std::uint32_t call_target_ = 0; std::size_t loop_count_ = 0;
    std::vector<MillenniumDosNinthFunctionByteEffect> effects_;
};
} // namespace eon
