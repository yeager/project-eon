#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace eon {

enum class MemoryTransferElementWidth : std::uint8_t {
    byte = 1,
    word = 2,
    longword = 4,
};

struct BoundedMemoryTransferContract {
    std::uint64_t instruction_address = 0;
    std::uint64_t source_base = 0;
    std::uint64_t destination_base = 0;
    std::uint64_t source_stride = 0;
    std::uint64_t destination_stride = 0;
    std::size_t element_count = 0;
    std::size_t maximum_chunk_elements = 0;
    MemoryTransferElementWidth element_width = MemoryTransferElementWidth::byte;
    // One past the largest address in the emulated address space. A zero
    // value denotes the complete uint64_t space.
    std::uint64_t address_limit_exclusive = 0;
    constexpr bool operator==(const BoundedMemoryTransferContract&) const = default;
};

struct BoundedMemoryTransferChunkObservation {
    std::uint64_t sequence = 0;
    std::uint64_t instruction_address = 0;
    std::size_t first_index = 0;
    std::uint64_t source_address = 0;
    std::uint64_t destination_address = 0;
    std::vector<std::uint32_t> values;
};

struct BoundedMemoryTransferEffect {
    std::size_t index = 0;
    std::uint64_t source_address = 0;
    std::uint64_t destination_address = 0;
    std::uint32_t value = 0;
    constexpr bool operator==(const BoundedMemoryTransferEffect&) const = default;
};

struct BoundedMemoryTransferCheckpoint {
    BoundedMemoryTransferContract contract;
    std::size_t next_index = 0;
    std::optional<std::uint64_t> last_sequence;
    bool complete = false;
    std::vector<BoundedMemoryTransferEffect> effects;
};

struct BoundedMemoryTransferObservationResult {
    bool accepted = false;
    std::string error;
};

// Observation-only ownership for a statically bounded copy loop. It never
// reads source memory, writes destination memory, or invents missing values.
class BoundedMemoryTransferSession {
public:
    explicit BoundedMemoryTransferSession(BoundedMemoryTransferContract contract);
    [[nodiscard]] BoundedMemoryTransferObservationResult observe_chunk(
        const BoundedMemoryTransferChunkObservation& observation);
    [[nodiscard]] BoundedMemoryTransferCheckpoint checkpoint() const;

private:
    BoundedMemoryTransferContract contract_;
    std::size_t next_index_ = 0;
    std::optional<std::uint64_t> last_sequence_;
    std::vector<BoundedMemoryTransferEffect> effects_;
};

} // namespace eon
