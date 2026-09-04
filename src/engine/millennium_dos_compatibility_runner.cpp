#include "engine/millennium_dos_compatibility_runner.hpp"

#include <stdexcept>

namespace eon {

MillenniumDosCompatibilityRunner::MillenniumDosCompatibilityRunner(
    const std::uint64_t generation, const std::uint64_t entry_sequence,
    const std::uint16_t code_segment)
    : generation_(generation), last_sequence_(entry_sequence), code_segment_(code_segment),
      paragraph_arena_(generation) {
    if (generation == 0 || entry_sequence == 0 || code_segment == 0)
        throw std::runtime_error("Invalid Millennium DOS compatibility process context");
}
bool MillenniumDosCompatibilityRunner::accepts(const std::uint64_t sequence) const {
    return sequence != 0 && sequence == last_sequence_ + 1;
}
void MillenniumDosCompatibilityRunner::commit(const std::uint64_t sequence) {
    if (!accepts(sequence)) throw std::runtime_error("Detached compatibility-runner sequence");
    last_sequence_ = sequence;
}
void MillenniumDosCompatibilityRunner::record_automatic_operation() {
    commit(next_sequence());
    ++automatic_operation_count_;
}
MillenniumDosParagraphAllocationResult
MillenniumDosCompatibilityRunner::allocate_paragraphs(const std::uint32_t paragraph_count) {
    auto result=paragraph_arena_.allocate(paragraph_count);
    if(result.allocation)record_automatic_operation();
    return result;
}
MillenniumDosCompatibilityRunnerCheckpoint MillenniumDosCompatibilityRunner::checkpoint(
    const MillenniumDosSoundDriverLoadState state,
    const MillenniumDosSoundDriverLoadBoundary boundary, std::string error) const {
    const bool terminal = state == MillenniumDosSoundDriverLoadState::title_exec_requested;
    return {generation_,last_sequence_,code_segment_,state,boundary,!terminal,terminal,
        compatibility_file_handle_,automatic_operation_count_,std::move(error),
        paragraph_arena_.checkpoint()};
}

} // namespace eon
