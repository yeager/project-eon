#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace eon {

enum class MillenniumDosExternalTransferKind {
    f9_short_return,
    f9_long_return,
    f2_reset_wrap_return,
    f2_tail_jump,
};

struct MillenniumDosExternalTransferContract {
    MillenniumDosExternalTransferKind kind;
    std::uint16_t source_instruction;
    std::uint16_t target_address;
    std::optional<std::uint16_t> terminal_return_instruction;
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
    case MillenniumDosExternalTransferKind::f9_short_return: return {kind,0x7381,0x73cc,0x73e6};
    case MillenniumDosExternalTransferKind::f9_long_return: return {kind,0x7381,0x73cc,0x740e};
    case MillenniumDosExternalTransferKind::f2_reset_wrap_return: return {kind,0x7228,0x702c,0x7040};
    case MillenniumDosExternalTransferKind::f2_tail_jump: return {kind,0x7253,0x0bdf,std::nullopt};
    }
    return {kind,0,0,std::nullopt};
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
            || !checkpoint_.contract.terminal_return_instruction
            || observation.sequence<=checkpoint_.entry->sequence
            || observation.return_instruction!=*checkpoint_.contract.terminal_return_instruction
            || observation.returned_to==0)
            return {false,"External transfer return does not match its entered contract"};
        checkpoint_.returned=observation; return {true,{}};
    }
    [[nodiscard]] MillenniumDosExternalTransferCheckpoint checkpoint() const { return checkpoint_; }
private:
    MillenniumDosExternalTransferCheckpoint checkpoint_;
};

} // namespace eon
