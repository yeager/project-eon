#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace eon {

enum class MillenniumDosParagraphProvenance : std::uint8_t {
    native_compatibility_arena,
};

struct MillenniumDosParagraphAllocation {
    std::uint64_t generation = 0;
    std::uint64_t allocation_id = 0;
    std::uint16_t segment = 0;
    std::uint16_t paragraph_count = 0;
    MillenniumDosParagraphProvenance provenance =
        MillenniumDosParagraphProvenance::native_compatibility_arena;
    constexpr bool operator==(const MillenniumDosParagraphAllocation&) const = default;
};

struct MillenniumDosParagraphArenaCheckpoint {
    std::uint64_t generation = 0;
    std::uint16_t begin_segment = 0;
    std::uint16_t end_segment_exclusive = 0;
    std::uint16_t next_segment = 0;
    std::vector<MillenniumDosParagraphAllocation> allocations;
};

struct MillenniumDosParagraphAllocationResult {
    std::optional<MillenniumDosParagraphAllocation> allocation;
    std::string error;
};

// A native segmented-address namespace used only by recovered DOS services.
// Its segment values are deterministic engine keys, never captured original
// DOS allocation results or assertions about original physical memory.
class MillenniumDosParagraphArena {
public:
    explicit MillenniumDosParagraphArena(std::uint64_t generation,
        std::uint16_t begin_segment = 0xe100,
        std::uint16_t end_segment_exclusive = 0xf000);
    [[nodiscard]] MillenniumDosParagraphAllocationResult allocate(
        std::uint32_t paragraph_count);
    [[nodiscard]] MillenniumDosParagraphArenaCheckpoint checkpoint() const;

private:
    std::uint64_t generation_ = 0;
    std::uint16_t begin_segment_ = 0;
    std::uint16_t end_segment_exclusive_ = 0;
    std::uint16_t next_segment_ = 0;
    std::uint64_t next_allocation_id_ = 1;
    std::vector<MillenniumDosParagraphAllocation> allocations_;
};

} // namespace eon
