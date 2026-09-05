#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace eon {

enum class MillenniumAmigaBootstrapRelocatorState {
    awaiting_overread_byte,
    awaiting_terminal_jump,
    awaiting_setup_call_return,
    awaiting_first_read_return,
    awaiting_opaque_first_stage,
    awaiting_first_stage_illegal_exception,
    awaiting_second_first_stage_illegal_exception,
    awaiting_first_stage_trace_exception,
    awaiting_first_stage_decrypted_instruction,
};

struct MillenniumAmigaBootstrapRelocatorBoundary {
    std::uint32_t instruction_address = 0;
    std::uint32_t source_address = 0;
    std::uint32_t target_address = 0;
    constexpr bool operator==(const MillenniumAmigaBootstrapRelocatorBoundary&) const = default;
};

struct MillenniumAmigaBootstrapRelocationByteEffect {
    std::uint32_t instruction_address = 0;
    std::uint32_t source_address = 0;
    std::uint32_t destination_address = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumAmigaBootstrapRelocationByteEffect&) const = default;
};

struct MillenniumAmigaBootstrapCustomChipEffect {
    std::uint32_t instruction_address = 0;
    std::uint32_t address = 0;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumAmigaBootstrapCustomChipEffect&) const = default;
};

struct MillenniumAmigaFirstStageRegisterObservation {
    std::uint32_t instruction_address = 0;
    std::array<std::uint32_t, 8> data{};
    std::array<std::uint32_t, 7> address{};
    std::uint32_t stack_pointer = 0;
    std::uint32_t exception_vector_10 = 0;
};

struct MillenniumAmigaFirstStageEntryExecution {
    std::uint32_t branch_target = 0;
    std::uint32_t snapshot_address = 0;
    std::array<std::uint32_t, 16> snapshot{};
    std::uint32_t transient_stack_address = 0;
    std::uint32_t original_a6 = 0;
    std::uint32_t installed_vector_address = 0;
    std::uint32_t installed_vector_value = 0;
    std::uint32_t resulting_d0 = 0;
    std::uint32_t resulting_a6 = 0;
    std::uint32_t resulting_stack_pointer = 0;
    std::uint32_t illegal_instruction_address = 0;
};

// Complete externally observed 68000 format-0 exception entry.  Eon does not
// synthesize the supervisor transition or exception frame.  The eight vector
// longs are the exact values read by MOVEM.L $8,D0-D7 after the handler has
// restored vector $10.
struct MillenniumAmigaFirstStageIllegalObservation {
    std::uint32_t handler_entry_address = 0;
    std::uint32_t exception_frame_address = 0;
    std::uint16_t saved_status_register = 0;
    std::uint32_t saved_program_counter = 0;
    std::array<std::uint32_t, 8> vector_longs{};
};

struct MillenniumAmigaFirstStageIllegalExecution {
    std::uint32_t exception_frame_address = 0;
    std::uint16_t saved_status_register = 0;
    std::uint32_t saved_program_counter = 0;
    std::uint32_t restored_vector_value = 0;
    std::uint32_t snapshot_address = 0;
    std::array<std::uint32_t, 8> snapshot{};
    std::uint32_t installed_vector_value = 0;
    std::uint32_t resulting_a0 = 0;
    std::uint32_t resulting_stack_pointer = 0;
    std::uint32_t illegal_instruction_address = 0;
};

struct MillenniumAmigaSecondIllegalObservation {
    std::uint32_t handler_entry_address = 0;
    std::uint32_t exception_frame_address = 0;
    std::uint16_t saved_status_register = 0;
    std::uint32_t saved_program_counter = 0;
};

struct MillenniumAmigaSecondIllegalExecution {
    std::uint32_t exception_frame_address = 0;
    std::uint16_t resulting_saved_status_register = 0;
    std::uint32_t resulting_saved_program_counter = 0;
    std::uint32_t temporary_stack_address = 0;
    std::array<std::uint32_t, 3> temporary_stack{};
    std::uint32_t vector_8_value = 0;
    std::uint32_t vector_9_value = 0;
    std::uint32_t cursor_address = 0;
    std::uint32_t cursor_value = 0;
    std::uint32_t saved_ciphertext_address = 0;
    std::uint32_t saved_ciphertext_value = 0;
    std::uint32_t transformed_address = 0;
    std::uint32_t transformed_value = 0;
    std::uint32_t resulting_stack_pointer = 0;
    std::uint32_t branch_target = 0;
    std::uint32_t trace_resume_address = 0;
};

// Complete externally observed format-0 trace exception entry. The live SR
// is distinct from the saved user frame and is therefore supplied explicitly;
// Eon does not synthesize the processor's exception transition.
struct MillenniumAmigaFirstTraceObservation {
    std::uint32_t handler_entry_address = 0;
    std::uint32_t exception_frame_address = 0;
    std::uint16_t handler_status_register = 0;
    std::uint16_t saved_status_register = 0;
    std::uint32_t saved_program_counter = 0;
};

struct MillenniumAmigaFirstTraceExecution {
    std::uint32_t exception_frame_address = 0;
    std::uint16_t resulting_handler_status_register = 0;
    std::uint16_t saved_status_register = 0;
    std::uint32_t saved_program_counter = 0;
    std::uint32_t temporary_stack_address = 0;
    std::array<std::uint32_t, 3> temporary_stack{};
    std::uint32_t restored_address = 0;
    std::uint32_t restored_value = 0;
    std::uint32_t cursor_address = 0;
    std::uint32_t cursor_value = 0;
    std::uint32_t saved_ciphertext_address = 0;
    std::uint32_t saved_ciphertext_value = 0;
    std::uint32_t key_source_address = 0;
    std::uint32_t key_source_value = 0;
    std::uint32_t xor_key = 0;
    std::uint32_t transformed_address = 0;
    std::uint32_t transformed_value = 0;
    std::uint32_t resulting_stack_pointer = 0;
};

struct MillenniumAmigaTraceBranchChainObservation {
    std::array<MillenniumAmigaFirstTraceObservation,10> exceptions{};
};
struct MillenniumAmigaTraceBranchChainExecution {
    std::uint32_t addx_instruction_address=0;
    std::uint32_t resulting_d2=0;
    std::uint16_t resulting_status_register=0;
    std::array<MillenniumAmigaFirstTraceExecution,10> decryptions{};
    std::uint32_t terminal_trace_program_counter=0;
};

// Manual recompilation of the exact, direct Defjam bootstrap relocator at
// $70000..$70041. The original DBRA reads one byte beyond the authenticated
// $400-byte load. That byte remains an explicit observation boundary.
class MillenniumAmigaBootstrapRelocatorSession {
public:
    explicit MillenniumAmigaBootstrapRelocatorSession(
        std::span<const std::uint8_t> disk_image);

    [[nodiscard]] MillenniumAmigaBootstrapRelocatorState state() const { return state_; }
    [[nodiscard]] MillenniumAmigaBootstrapRelocatorBoundary boundary() const;
    [[nodiscard]] const std::vector<MillenniumAmigaBootstrapRelocationByteEffect>&
    copy_effects() const { return copy_effects_; }
    [[nodiscard]] const MillenniumAmigaBootstrapCustomChipEffect&
    custom_chip_effect() const { return custom_chip_effect_; }
    [[nodiscard]] std::uint32_t final_a3() const { return final_a3_; }
    [[nodiscard]] std::uint32_t final_a5() const { return final_a5_; }
    [[nodiscard]] std::uint32_t final_d1() const { return final_d1_; }
    [[nodiscard]] std::span<const std::uint8_t> first_stage_bytes() const {
        return first_stage_bytes_;
    }
    [[nodiscard]] const std::string& first_stage_sha256() const {
        return first_stage_sha256_;
    }

    void observe_overread_byte(std::uint32_t instruction_address,
        std::uint32_t source_address, std::uint8_t value);
    void observe_terminal_jump(std::uint32_t instruction_address,
        std::uint32_t target_address);
    void observe_setup_call_return(std::uint32_t instruction_address,
        std::uint32_t target_address);
    void observe_first_read_return(std::uint32_t instruction_address,
        std::uint32_t target_address, std::uint8_t io_error);
    [[nodiscard]] MillenniumAmigaFirstStageEntryExecution
    execute_first_stage_entry(const MillenniumAmigaFirstStageRegisterObservation&);
    [[nodiscard]] const std::optional<MillenniumAmigaFirstStageEntryExecution>&
    first_stage_entry_execution() const { return first_stage_entry_execution_; }
    [[nodiscard]] MillenniumAmigaFirstStageIllegalExecution
    execute_first_stage_illegal_handler(const MillenniumAmigaFirstStageIllegalObservation&);
    [[nodiscard]] const std::optional<MillenniumAmigaFirstStageIllegalExecution>&
    first_stage_illegal_execution() const { return first_stage_illegal_execution_; }
    [[nodiscard]] MillenniumAmigaSecondIllegalExecution
    execute_second_illegal_handler(const MillenniumAmigaSecondIllegalObservation&);
    [[nodiscard]] const std::optional<MillenniumAmigaSecondIllegalExecution>&
    second_illegal_execution() const { return second_illegal_execution_; }
    [[nodiscard]] MillenniumAmigaFirstTraceExecution
    execute_first_trace_handler(const MillenniumAmigaFirstTraceObservation&);
    [[nodiscard]] const std::optional<MillenniumAmigaFirstTraceExecution>&
    first_trace_execution() const { return first_trace_execution_; }
    [[nodiscard]] MillenniumAmigaTraceBranchChainExecution
    execute_trace_branch_chain(const MillenniumAmigaTraceBranchChainObservation&);
    [[nodiscard]] const std::optional<MillenniumAmigaTraceBranchChainExecution>&
    trace_branch_chain_execution() const { return trace_branch_chain_execution_; }

private:
    MillenniumAmigaBootstrapRelocatorState state_ =
        MillenniumAmigaBootstrapRelocatorState::awaiting_overread_byte;
    MillenniumAmigaBootstrapCustomChipEffect custom_chip_effect_;
    std::vector<MillenniumAmigaBootstrapRelocationByteEffect> copy_effects_;
    std::uint32_t final_a3_ = 0;
    std::uint32_t final_a5_ = 0;
    std::uint32_t final_d1_ = 0;
    std::vector<std::uint8_t> first_stage_bytes_;
    std::string first_stage_sha256_;
    std::optional<MillenniumAmigaFirstStageEntryExecution> first_stage_entry_execution_;
    std::optional<MillenniumAmigaFirstStageIllegalExecution> first_stage_illegal_execution_;
    std::optional<MillenniumAmigaSecondIllegalExecution> second_illegal_execution_;
    std::optional<MillenniumAmigaFirstTraceExecution> first_trace_execution_;
    std::optional<MillenniumAmigaTraceBranchChainExecution> trace_branch_chain_execution_;
};

} // namespace eon
