#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace eon {

enum class MillenniumDosTitleInitializationState {
    awaiting_entry,
    private_interrupt_result_boundary,
    selected_local_call_boundary,
    selected_callee_private_interrupt_result_boundary,
};

enum class MillenniumDosTitleInitializationEffectWidth { byte, word };

struct MillenniumDosTitlePrivateInterruptResultObservation {
    std::uint64_t sequence = 0;
    std::uint16_t interrupt_address = 0;
    std::uint16_t return_address = 0;
    std::uint16_t ax = 0;
    std::uint16_t flags = 0;
};

struct MillenniumDosTitleInitializationMemoryEffect {
    std::uint16_t instruction_address = 0;
    std::uint16_t offset = 0;
    MillenniumDosTitleInitializationEffectWidth width =
        MillenniumDosTitleInitializationEffectWidth::byte;
    std::uint16_t value = 0;
};

struct MillenniumDosTitleInitializationRegisterEffect {
    std::uint16_t instruction_address = 0;
    std::string_view register_name;
    std::uint16_t value = 0;
};

struct MillenniumDosTitlePrivateInterruptBoundary {
    std::uint16_t call_address = 0;
    std::uint16_t wrapper_address = 0;
    std::uint16_t interrupt_address = 0;
    std::uint8_t interrupt = 0;
    std::uint16_t function = 0;
    std::uint16_t record_segment = 0;
    std::uint16_t record_offset = 0;
    bool result_observed = false;
    bool stack_storage_modeled = false;
};

struct MillenniumDosTitleInitializationCheckpoint {
    MillenniumDosTitleInitializationState state =
        MillenniumDosTitleInitializationState::awaiting_entry;
    std::uint64_t last_sequence = 0;
    std::uint16_t child_code_segment = 0;
    std::vector<MillenniumDosTitleInitializationRegisterEffect> register_effects;
    std::vector<MillenniumDosTitleInitializationMemoryEffect> memory_effects;
    MillenniumDosTitlePrivateInterruptBoundary boundary;
    std::uint16_t observed_ax = 0;
    std::uint16_t observed_flags = 0;
    std::uint8_t selected_mode = 0;
    std::uint16_t selected_call_address = 0;
    std::uint16_t selected_call_target = 0;
    MillenniumDosTitlePrivateInterruptBoundary selected_callee_boundary;
};

// Native execution of TITLES.EXE's deterministic $1b80 startup through the
// exact $0122 wrapper request. It records instruction-defined register state
// and stops before INT 91h can return. The wrapper's x86 stack storage is not
// synthesized because the narrow compatibility child allocation does not
// establish the original DOS process memory extent.
class MillenniumDosTitleInitializationSession {
public:
    MillenniumDosTitleInitializationSession(
        std::span<const std::uint8_t> titles_executable,
        std::uint16_t child_code_segment, std::uint64_t entry_sequence);

    void execute_exact_startup(std::uint64_t sequence,
        std::uint16_t entry_address, std::uint16_t call_address,
        std::uint16_t wrapper_address, std::uint8_t interrupt);
    void observe_private_interrupt_result(
        const MillenniumDosTitlePrivateInterruptResultObservation&);
    void execute_selected_callee_start(std::uint64_t sequence,
        std::uint16_t selected_call_address, std::uint16_t selected_call_target);

    [[nodiscard]] MillenniumDosTitleInitializationCheckpoint checkpoint() const;

private:
    MillenniumDosTitleInitializationState state_ =
        MillenniumDosTitleInitializationState::awaiting_entry;
    std::uint64_t last_sequence_ = 0;
    std::uint16_t child_code_segment_ = 0;
    std::vector<MillenniumDosTitleInitializationRegisterEffect> effects_;
    std::vector<MillenniumDosTitleInitializationMemoryEffect> memory_effects_;
    MillenniumDosTitlePrivateInterruptBoundary boundary_;
    MillenniumDosTitlePrivateInterruptBoundary selected_callee_boundary_;
    std::uint16_t observed_ax_ = 0;
    std::uint16_t observed_flags_ = 0;
    std::uint8_t selected_mode_ = 0;
    std::uint16_t selected_call_address_ = 0;
    std::uint16_t selected_call_target_ = 0;
};

} // namespace eon
