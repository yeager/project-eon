#pragma once

#include "engine/millennium_dos_sound_driver_load_session.hpp"
#include "engine/millennium_dos_paragraph_arena.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace eon {

enum class MillenniumDosTitleInitializationState;

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
    MillenniumDosParagraphArenaCheckpoint title_paragraph_arena;
    std::optional<MillenniumDosTitleInitializationState> title_state;
};

// Native, leaf-backed implementation of the deterministic DOS file service
// used by the recovered selected-driver chain. The private handle identifies
// the already admitted immutable leaf; it is not claimed as an observed DOS
// handle. Its allocation and INT $95 vector installation belong to Eon's
// process-local compatibility state. Parent stack state remains an external
// boundary; child EXEC admission uses the separate bounded child service.
class MillenniumDosCompatibilityRunner {
public:
  MillenniumDosCompatibilityRunner(std::uint64_t generation,
                                   std::uint64_t entry_sequence,
                                   std::uint16_t code_segment);
  [[nodiscard]] bool accepts(std::uint64_t sequence) const;
  void commit(std::uint64_t sequence);
  [[nodiscard]] std::uint64_t next_sequence() const {
    return last_sequence_ + 1;
  }
  [[nodiscard]] std::uint16_t compatibility_file_handle() const {
    return compatibility_file_handle_;
  }
  void record_automatic_operation();
  void synchronize_external_sequence(std::uint64_t sequence);
  [[nodiscard]] MillenniumDosParagraphAllocationResult
  allocate_paragraphs(std::uint32_t paragraph_count);
  [[nodiscard]] MillenniumDosParagraphAllocationResult
  allocate_title_paragraphs(std::uint32_t paragraph_count);
  [[nodiscard]] std::uint16_t remaining_title_paragraphs() const;
  [[nodiscard]] std::uint16_t code_segment() const { return code_segment_; }
  [[nodiscard]] MillenniumDosCompatibilityRunnerCheckpoint
  checkpoint(MillenniumDosSoundDriverLoadState state,
             MillenniumDosSoundDriverLoadBoundary boundary,
             std::string error = {}) const;

private:
  std::uint64_t generation_ = 0;
  std::uint64_t last_sequence_ = 0;
  std::uint16_t code_segment_ = 0;
  std::uint16_t compatibility_file_handle_ = 0xe001;
  std::uint64_t automatic_operation_count_ = 0;
  MillenniumDosParagraphArena paragraph_arena_;
  MillenniumDosParagraphArena title_paragraph_arena_;
};

} // namespace eon
