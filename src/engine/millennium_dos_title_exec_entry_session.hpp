#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace eon {

enum class MillenniumDosTitleExecEntryState {
    awaiting_child_process_entry,
    entry_prefix_boundary,
    title_entry_boundary,
};

enum class MillenniumDosTitleExecEntryProvenance {
    observed_process_entry,
    eon_dos_compatibility_service,
};

struct MillenniumDosTitleExecProcessEntry {
    std::uint64_t sequence = 0;
    std::uint16_t parent_exec_instruction = 0;
    std::uint16_t ax = 0;
    std::uint16_t dx = 0;
    std::uint16_t parameter_block = 0;
    std::uint16_t child_entry_ip = 0;
    std::uint16_t child_code_segment = 0;
    MillenniumDosTitleExecEntryProvenance provenance =
        MillenniumDosTitleExecEntryProvenance::observed_process_entry;
};

struct MillenniumDosTitleExecRegisterEffect {
    std::uint16_t instruction_address = 0;
    std::string_view register_name;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosTitleExecRegisterEffect&) const = default;
};

struct MillenniumDosTitleExecEntryCheckpoint {
    MillenniumDosTitleExecEntryState state =
        MillenniumDosTitleExecEntryState::awaiting_child_process_entry;
    std::uint64_t last_sequence = 0;
    std::uint16_t child_code_segment = 0;
    MillenniumDosTitleExecEntryProvenance provenance =
        MillenniumDosTitleExecEntryProvenance::observed_process_entry;
    // Value-owned so diagnostics can cross the coordinator/controller/host
    // layers without borrowing the mutable session's backing vector.
    std::vector<MillenniumDosTitleExecRegisterEffect> register_effects;
};

// Connects MILL.COM's exact TITLES.EXE EXEC request to the flat child's exact
// $0100 entry prefix. Creating the DOS process remains an explicit external
// observation or a labelled Eon compatibility-service result. This class does
// not synthesize a DOS success return or execute any title logic past $1b80.
class MillenniumDosTitleExecEntrySession {
public:
    MillenniumDosTitleExecEntrySession(std::span<const std::uint8_t> mill_com,
        std::span<const std::uint8_t> titles_executable);

    void observe_child_process_entry(const MillenniumDosTitleExecProcessEntry&);
    void execute_exact_entry_prefix(std::uint64_t sequence,
        std::uint16_t prefix_address, std::uint16_t jump_address,
        std::uint16_t jump_destination);

    [[nodiscard]] MillenniumDosTitleExecEntryCheckpoint checkpoint() const;
    [[nodiscard]] MillenniumDosTitleExecEntryState state() const { return state_; }

private:
    MillenniumDosTitleExecEntryState state_ =
        MillenniumDosTitleExecEntryState::awaiting_child_process_entry;
    std::uint64_t last_sequence_ = 0;
    std::uint16_t child_code_segment_ = 0;
    MillenniumDosTitleExecEntryProvenance provenance_ =
        MillenniumDosTitleExecEntryProvenance::observed_process_entry;
    std::vector<MillenniumDosTitleExecRegisterEffect> effects_;
};

} // namespace eon
