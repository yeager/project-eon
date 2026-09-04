#include "engine/millennium_dos_paragraph_arena.hpp"

#include <stdexcept>

namespace eon {

MillenniumDosParagraphArena::MillenniumDosParagraphArena(
    const std::uint64_t generation, const std::uint16_t begin_segment,
    const std::uint16_t end_segment_exclusive)
    : generation_(generation), begin_segment_(begin_segment),
      end_segment_exclusive_(end_segment_exclusive), next_segment_(begin_segment) {
    if (generation == 0 || begin_segment == 0
        || end_segment_exclusive <= begin_segment) {
        throw std::runtime_error("Invalid Millennium DOS compatibility paragraph arena");
    }
}

MillenniumDosParagraphAllocationResult MillenniumDosParagraphArena::allocate(
    const std::uint32_t paragraph_count) {
    if (paragraph_count == 0 || paragraph_count > 0xffffU) {
        return {std::nullopt,"DOS compatibility allocation has invalid paragraph count"};
    }
    const auto next = static_cast<std::uint32_t>(next_segment_);
    const auto end = static_cast<std::uint32_t>(end_segment_exclusive_);
    if (paragraph_count > end - next) {
        return {std::nullopt,"DOS compatibility paragraph arena is exhausted"};
    }
    const auto allocation = MillenniumDosParagraphAllocation{
        generation_,next_allocation_id_++,next_segment_,
        static_cast<std::uint16_t>(paragraph_count),
        MillenniumDosParagraphProvenance::native_compatibility_arena};
    next_segment_=static_cast<std::uint16_t>(next+paragraph_count);
    allocations_.push_back(allocation);
    return {allocation,{}};
}

MillenniumDosParagraphArenaCheckpoint MillenniumDosParagraphArena::checkpoint() const {
    return {generation_,begin_segment_,end_segment_exclusive_,next_segment_,allocations_};
}

} // namespace eon
