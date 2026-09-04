#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace eon {

enum class MillenniumDosTitleToGameState {
    awaiting_title_cleanup_return,
    awaiting_title_post_driver_return,
    awaiting_stack_word,
    awaiting_title_final_local_return,
    awaiting_exit_stub_call_return,
    awaiting_title_termination,
    awaiting_parent_exec_return,
    awaiting_child_status,
    game_exec_boundary,
};

enum class MillenniumDosTitleToGameBoundaryKind {
    call_return,
    runtime_word,
    dos_interrupt_return,
    dos_exec,
};

struct MillenniumDosTitleToGameBoundary {
    MillenniumDosTitleToGameBoundaryKind kind =
        MillenniumDosTitleToGameBoundaryKind::call_return;
    std::uint16_t instruction_address = 0;
    std::uint16_t target_or_return = 0;
    std::uint16_t dx = 0;
    std::uint16_t ax = 0;
    constexpr bool operator==(const MillenniumDosTitleToGameBoundary&) const = default;
};

struct MillenniumDosTitleToGameByteEffect {
    std::uint16_t instruction_address = 0;
    std::uint16_t address = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumDosTitleToGameByteEffect&) const = default;
};

// Exact local title-exit and parent-launcher continuation. Native calls and
// DOS results remain explicit observations; reaching the final state proves
// only the original 2200ad.exe EXEC request, not successful game startup.
class MillenniumDosTitleToGameSession {
public:
    MillenniumDosTitleToGameSession(std::span<const std::uint8_t> mill_launcher,
        std::span<const std::uint8_t> titles_executable);

    [[nodiscard]] MillenniumDosTitleToGameState state() const { return state_; }
    [[nodiscard]] MillenniumDosTitleToGameBoundary boundary() const;
    [[nodiscard]] const std::vector<MillenniumDosTitleToGameByteEffect>& effects() const {
        return effects_;
    }
    [[nodiscard]] std::uint16_t restored_stack_pointer() const { return restored_sp_; }
    [[nodiscard]] std::uint8_t child_status_al() const { return child_status_al_; }

    void observe_call_return(std::uint16_t call_address, std::uint16_t return_address);
    void observe_stack_word(std::uint16_t instruction_address,
        std::uint16_t address, std::uint16_t value);
    void observe_title_termination(std::uint16_t interrupt_address,
        std::uint16_t ax);
    void observe_parent_exec_return(std::uint16_t interrupt_address,
        bool carry);
    void observe_child_status(std::uint16_t interrupt_address,
        std::uint8_t al, bool carry);

private:
    MillenniumDosTitleToGameState state_ =
        MillenniumDosTitleToGameState::awaiting_title_cleanup_return;
    std::vector<MillenniumDosTitleToGameByteEffect> effects_;
    std::uint16_t restored_sp_ = 0;
    std::uint8_t child_status_al_ = 0;
};

} // namespace eon
