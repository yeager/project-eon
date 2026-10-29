#include "engine/millennium_dos_compatibility_runner.hpp"

#include <stdexcept>

namespace eon {

MillenniumDosCompatibilityRunner::MillenniumDosCompatibilityRunner(
    const std::uint64_t generation, const std::uint64_t entry_sequence,
    const std::uint16_t code_segment)
    : generation_(generation), last_sequence_(entry_sequence),
      code_segment_(code_segment), paragraph_arena_(generation),
      title_paragraph_arena_(generation, 0x3000, 0xd000) {
  if (generation == 0 || entry_sequence == 0 || code_segment == 0)
    throw std::runtime_error(
        "Invalid Millennium DOS compatibility process context");
}
bool MillenniumDosCompatibilityRunner::accepts(
    const std::uint64_t sequence) const {
  return sequence != 0 && sequence == last_sequence_ + 1;
}
void MillenniumDosCompatibilityRunner::commit(const std::uint64_t sequence) {
  if (!accepts(sequence))
    throw std::runtime_error("Detached compatibility-runner sequence");
  last_sequence_ = sequence;
}
void MillenniumDosCompatibilityRunner::record_automatic_operation() {
  commit(next_sequence());
  ++automatic_operation_count_;
}
void MillenniumDosCompatibilityRunner::synchronize_external_sequence(
    const std::uint64_t sequence) {
  if (sequence < last_sequence_)
    throw std::runtime_error("Stale compatibility external sequence");
  last_sequence_ = sequence;
}
MillenniumDosParagraphAllocationResult
MillenniumDosCompatibilityRunner::allocate_paragraphs(
    const std::uint32_t paragraph_count) {
  auto result = paragraph_arena_.allocate(paragraph_count);
  if (result.allocation)
    record_automatic_operation();
  return result;
}
MillenniumDosParagraphAllocationResult
MillenniumDosCompatibilityRunner::allocate_title_paragraphs(
    const std::uint32_t paragraph_count) {
  auto result = title_paragraph_arena_.allocate(paragraph_count);
  if (result.allocation)
    record_automatic_operation();
  return result;
}
std::uint16_t
MillenniumDosCompatibilityRunner::remaining_title_paragraphs() const {
  const auto arena = title_paragraph_arena_.checkpoint();
  return static_cast<std::uint16_t>(arena.end_segment_exclusive -
                                    arena.next_segment);
}
MillenniumDosVirtualVectorTarget
MillenniumDosCompatibilityRunner::get_virtual_vector(const std::uint8_t vector) {
  auto& target = virtual_ivt_[vector];
  if (!target.initialized) {
    if (!opaque_handler_allocation_) {
      const auto allocation = allocate_paragraphs(1);
      if (!allocation.allocation)
        throw std::runtime_error("DOS virtual IVT handler allocation failed: " + allocation.error);
      opaque_handler_allocation_ = *allocation.allocation;
    }
    // This points into Eon's generation-owned, opaque handler slot. It is a
    // virtual DOS address, not a historical BIOS/DOS vector or executable
    // machine-code implementation. Invocation remains an explicit boundary.
    target = {0, opaque_handler_allocation_->segment, true};
  }
  return target;
}
void MillenniumDosCompatibilityRunner::set_virtual_vector(
    const std::uint8_t vector, const std::uint16_t offset, const std::uint16_t segment) {
  virtual_ivt_[vector] = {offset, segment, true};
}
MillenniumDosCompatibilityRunnerCheckpoint
MillenniumDosCompatibilityRunner::checkpoint(
    const MillenniumDosSoundDriverLoadState state,
    const MillenniumDosSoundDriverLoadBoundary boundary,
    std::string error) const {
  const bool terminal =
      state == MillenniumDosSoundDriverLoadState::title_exec_requested;
  return {generation_,
          last_sequence_,
          code_segment_,
          state,
          boundary,
          !terminal,
          terminal,
          compatibility_file_handle_,
          automatic_operation_count_,
          std::move(error),
          paragraph_arena_.checkpoint(),
          title_paragraph_arena_.checkpoint(),
          std::nullopt};
}

} // namespace eon
