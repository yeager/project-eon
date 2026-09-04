#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace eon {

enum class MillenniumDosExternalTransferKind {
    f9_short_return,
    f9_long_return,
    f2_reset_wrap_return,
    f2_tail_jump,
    f2_tail_active_return,
    bdf_mode_two_jump,
    bdf_other_mode_jump,
};

struct MillenniumDosExternalTransferContract {
    MillenniumDosExternalTransferKind kind;
    std::uint16_t source_instruction;
    std::uint16_t target_address;
    std::array<std::uint16_t,4> terminal_return_instructions{};
    std::size_t terminal_return_count = 0;
};

struct MillenniumDosExternalTransferEntryObservation {
    std::uint64_t sequence = 0;
    std::uint16_t source_instruction = 0;
    std::uint16_t target_address = 0;
};

struct MillenniumDosExternalTransferReturnObservation {
    std::uint64_t sequence = 0;
    std::uint16_t return_instruction = 0;
    std::uint16_t returned_to = 0;
};

struct MillenniumDosExternalTransferCheckpoint {
    MillenniumDosExternalTransferContract contract;
    std::optional<MillenniumDosExternalTransferEntryObservation> entry;
    std::optional<MillenniumDosExternalTransferReturnObservation> returned;
};

struct MillenniumDosExternalTransferResult { bool accepted=false; std::string error; };

[[nodiscard]] constexpr MillenniumDosExternalTransferContract
millennium_dos_external_transfer_contract(const MillenniumDosExternalTransferKind kind) {
    switch (kind) {
    case MillenniumDosExternalTransferKind::f9_short_return: return {kind,0x7381,0x73cc,{0x73e6},1};
    case MillenniumDosExternalTransferKind::f9_long_return: return {kind,0x7381,0x73cc,{0x740e},1};
    case MillenniumDosExternalTransferKind::f2_reset_wrap_return: return {kind,0x7228,0x702c,{0x7040},1};
    case MillenniumDosExternalTransferKind::f2_tail_jump: return {kind,0x7253,0x0bdf,{},0};
    case MillenniumDosExternalTransferKind::f2_tail_active_return: return {kind,0x7253,0x0bdf,{0x0be6},1};
    case MillenniumDosExternalTransferKind::bdf_mode_two_jump: return {kind,0x0c4b,0x11f7,{0x12cb,0x129c},2};
    case MillenniumDosExternalTransferKind::bdf_other_mode_jump:
        return {kind,0x0c4e,0x0caa,{0x0d67,0x0e53,0x0e28,0x0d3d},4};
    }
    return {kind,0,0,{},0};
}

class MillenniumDosExternalTransferAdmission {
public:
    explicit MillenniumDosExternalTransferAdmission(MillenniumDosExternalTransferKind kind)
        : checkpoint_{millennium_dos_external_transfer_contract(kind),{}, {}} {}

    [[nodiscard]] MillenniumDosExternalTransferResult observe_entry(
        const MillenniumDosExternalTransferEntryObservation& observation) {
        if (checkpoint_.entry || observation.sequence==0
            || observation.source_instruction!=checkpoint_.contract.source_instruction
            || observation.target_address!=checkpoint_.contract.target_address)
            return {false,"External transfer entry does not match its exact contract"};
        checkpoint_.entry=observation; return {true,{}};
    }
    [[nodiscard]] MillenniumDosExternalTransferResult observe_return(
        const MillenniumDosExternalTransferReturnObservation& observation) {
        if (!checkpoint_.entry || checkpoint_.returned
            || checkpoint_.contract.terminal_return_count==0
            || observation.sequence<=checkpoint_.entry->sequence
            || !matches_terminal_return(observation.return_instruction)
            || observation.returned_to==0)
            return {false,"External transfer return does not match its entered contract"};
        checkpoint_.returned=observation; return {true,{}};
    }
    [[nodiscard]] MillenniumDosExternalTransferCheckpoint checkpoint() const { return checkpoint_; }
private:
    [[nodiscard]] bool matches_terminal_return(const std::uint16_t instruction) const {
        for (std::size_t index=0;index<checkpoint_.contract.terminal_return_count;++index) {
            if (checkpoint_.contract.terminal_return_instructions[index]==instruction) return true;
        }
        return false;
    }
    MillenniumDosExternalTransferCheckpoint checkpoint_;
};

} // namespace eon
