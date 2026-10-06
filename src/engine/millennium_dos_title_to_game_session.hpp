#pragma once

#include <cstdint>
#include <span>
#include <string_view>
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
    awaiting_game_process_entry,
    game_process_entry_boundary,
    game_entry_prefix_executed,
    game_startup_call_boundary,
};

enum class MillenniumDosTitleToGameBoundaryKind {
    call_return,
    runtime_word,
    dos_interrupt_return,
    dos_exec,
    game_process_entry,
    game_entry_prefix,
    game_native_call,
};

struct MillenniumDosTitleToGameRegisterEffect {
    std::uint16_t instruction_address = 0;
    std::string_view register_name;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosTitleToGameRegisterEffect&) const = default;
};

struct MillenniumDosTitleToGameStackWordEffect {
    std::uint16_t instruction_address = 0;
    std::uint16_t segment = 0;
    std::uint16_t offset = 0;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosTitleToGameStackWordEffect&) const = default;
};

enum class MillenniumDosTitleToGameEntryProvenance {
    unspecified,
    observed_process_entry,
    eon_dos_compatibility_service,
};

struct MillenniumDosTitleToGameProcessEntry {
    std::uint64_t sequence = 0;
    std::uint16_t call_instruction = 0;
    std::uint16_t exec_interrupt_instruction = 0;
    std::uint16_t ax = 0;
    std::uint16_t dx = 0;
    std::uint16_t parameter_block = 0;
    std::uint16_t child_entry_ip = 0;
    std::uint16_t child_code_segment = 0;
    std::uint16_t initial_stack_segment = 0;
    std::uint16_t initial_stack_pointer = 0;
    MillenniumDosTitleToGameEntryProvenance provenance =
        MillenniumDosTitleToGameEntryProvenance::unspecified;
};

struct MillenniumDosTitleToGameExecRequest {
    std::uint64_t sequence = 0;
    std::uint16_t call_instruction = 0;
    std::uint16_t helper_target = 0;
    std::uint16_t program_address = 0;
    std::uint16_t parameter_block = 0;
};

struct MillenniumDosTitleToGameBoundary {
    MillenniumDosTitleToGameBoundaryKind kind =
        MillenniumDosTitleToGameBoundaryKind::call_return;
    std::uint16_t instruction_address = 0;
    std::uint16_t target_or_return = 0;
    std::uint16_t dx = 0;
    std::uint16_t ax = 0;
    std::uint16_t bx = 0;
    constexpr bool operator==(const MillenniumDosTitleToGameBoundary&) const = default;
};

struct MillenniumDosTitleToGameByteEffect {
    std::uint16_t instruction_address = 0;
    std::uint16_t address = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumDosTitleToGameByteEffect&) const = default;
};

// Exact local title-exit and parent-launcher continuation. Native calls and
// DOS results remain explicit observations; game execution advances only
// after the typed DOS child-entry transfer is recorded.
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
    [[nodiscard]] std::uint64_t child_entry_sequence() const { return child_entry_sequence_; }
    [[nodiscard]] std::uint64_t game_exec_sequence() const { return game_exec_sequence_; }
    [[nodiscard]] std::uint16_t child_code_segment() const { return child_code_segment_; }
    [[nodiscard]] std::uint16_t initial_stack_segment() const { return initial_stack_segment_; }
    [[nodiscard]] std::uint16_t initial_stack_pointer() const { return initial_stack_pointer_; }
    [[nodiscard]] MillenniumDosTitleToGameEntryProvenance child_entry_provenance() const {
        return child_entry_provenance_;
    }
    [[nodiscard]] const std::vector<MillenniumDosTitleToGameRegisterEffect>& register_effects() const {
        return register_effects_;
    }
    [[nodiscard]] const std::vector<MillenniumDosTitleToGameStackWordEffect>& stack_word_effects() const {
        return stack_word_effects_;
    }

    void observe_call_return(std::uint16_t call_address, std::uint16_t return_address);
    void observe_stack_word(std::uint16_t instruction_address,
        std::uint16_t address, std::uint16_t value);
    void observe_title_termination(std::uint16_t interrupt_address,
        std::uint16_t ax);
    void observe_game_exec_request(const MillenniumDosTitleToGameExecRequest&);
    void observe_parent_exec_return(std::uint16_t interrupt_address,
        bool carry);
    void observe_child_status(std::uint16_t interrupt_address,
        std::uint8_t al, bool carry);
    void observe_game_process_entry(const MillenniumDosTitleToGameProcessEntry&);
    void execute_game_entry_prefix(std::span<const std::uint8_t> game_executable);
    void execute_game_startup_prefix(std::span<const std::uint8_t> game_executable);
    [[nodiscard]] bool parent_exec_returned() const { return parent_exec_returned_; }
    [[nodiscard]] bool child_status_observed() const { return child_status_observed_; }

private:
    MillenniumDosTitleToGameState state_ =
        MillenniumDosTitleToGameState::awaiting_title_cleanup_return;
    std::vector<MillenniumDosTitleToGameByteEffect> effects_;
    std::uint16_t restored_sp_ = 0;
    std::uint8_t child_status_al_ = 0;
    bool parent_exec_returned_ = false;
    bool child_status_observed_ = false;
    bool game_exec_requested_ = false;
    std::uint64_t game_exec_sequence_ = 0;
    std::uint64_t child_entry_sequence_ = 0;
    std::uint16_t child_code_segment_ = 0;
    std::uint16_t initial_stack_segment_ = 0;
    std::uint16_t initial_stack_pointer_ = 0;
    MillenniumDosTitleToGameEntryProvenance child_entry_provenance_ =
        MillenniumDosTitleToGameEntryProvenance::unspecified;
    std::vector<MillenniumDosTitleToGameRegisterEffect> register_effects_;
    std::vector<MillenniumDosTitleToGameStackWordEffect> stack_word_effects_;
};

} // namespace eon
