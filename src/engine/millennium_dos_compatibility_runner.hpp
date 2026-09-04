#pragma once

#include "engine/millennium_dos_sound_driver_load_session.hpp"
#include "engine/millennium_dos_paragraph_arena.hpp"

#include <cstdint>
#include <string>

namespace eon {

struct MillenniumDosCompatibilityRunnerCheckpoint {
    std::uint64_t generation = 0;
    std::uint64_t last_sequence = 0;
    std::uint16_t code_segment = 0;
    MillenniumDosSoundDriverLoadState state =
        MillenniumDosSoundDriverLoadState::awaiting_open_result;
    MillenniumDosSoundDriverLoadBoundary boundary;
    bool external_result_required = true;
    bool title_exec_requested = false;
    std::uint16_t compatibility_file_handle = 0;
    std::uint64_t automatic_operation_count = 0;
    std::string error;
    MillenniumDosParagraphArenaCheckpoint paragraph_arena;
};

// Native, leaf-backed implementation of the deterministic DOS file service
// used by the recovered selected-driver chain. The private handle identifies
// the already admitted immutable leaf; it is not claimed as an observed DOS
// handle. Allocation, interrupt-vector state, parent stack state and EXEC
// remain explicit evidence boundaries.
class MillenniumDosCompatibilityRunner {
public:
    MillenniumDosCompatibilityRunner(std::uint64_t generation,
        std::uint64_t entry_sequence, std::uint16_t code_segment);
    [[nodiscard]] bool accepts(std::uint64_t sequence) const;
    void commit(std::uint64_t sequence);
    [[nodiscard]] std::uint64_t next_sequence() const { return last_sequence_ + 1; }
    [[nodiscard]] std::uint16_t compatibility_file_handle() const {
        return compatibility_file_handle_;
    }
    void record_automatic_operation();
    [[nodiscard]] MillenniumDosParagraphAllocationResult allocate_paragraphs(
        std::uint32_t paragraph_count);
    [[nodiscard]] MillenniumDosCompatibilityRunnerCheckpoint checkpoint(
        MillenniumDosSoundDriverLoadState state,
        MillenniumDosSoundDriverLoadBoundary boundary,
        std::string error = {}) const;
private:
    std::uint64_t generation_ = 0;
    std::uint64_t last_sequence_ = 0;
    std::uint16_t code_segment_ = 0;
    std::uint16_t compatibility_file_handle_ = 0xe001;
    std::uint64_t automatic_operation_count_ = 0;
    MillenniumDosParagraphArena paragraph_arena_;
};

} // namespace eon
