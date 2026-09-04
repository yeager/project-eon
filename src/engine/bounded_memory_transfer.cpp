#include "engine/bounded_memory_transfer.hpp"

#include <limits>
#include <stdexcept>

namespace eon {
namespace {

[[nodiscard]] std::uint64_t checked_address(const std::uint64_t base,
    const std::uint64_t stride, const std::size_t index,
    const std::uint64_t width, const std::uint64_t limit) {
    const auto index64 = static_cast<std::uint64_t>(index);
    if (index64 != index || (index64 != 0 && stride > (std::numeric_limits<std::uint64_t>::max() - base) / index64)) {
        throw std::invalid_argument("Memory-transfer address arithmetic overflows");
    }
    const auto address = base + stride * index64;
    if (width > std::numeric_limits<std::uint64_t>::max() - address) {
        throw std::invalid_argument("Memory-transfer element end overflows");
    }
    const auto end = address + width;
    if (limit != 0 && end > limit) {
        throw std::invalid_argument("Memory-transfer range exceeds its address space");
    }
    return address;
}

[[nodiscard]] std::uint32_t maximum_value(const MemoryTransferElementWidth width) {
    switch (width) {
    case MemoryTransferElementWidth::byte: return 0xffU;
    case MemoryTransferElementWidth::word: return 0xffffU;
    case MemoryTransferElementWidth::longword: return 0xffffffffU;
    }
    throw std::invalid_argument("Unsupported memory-transfer element width");
}

} // namespace

BoundedMemoryTransferSession::BoundedMemoryTransferSession(
    BoundedMemoryTransferContract contract) : contract_(contract) {
    const auto width = static_cast<std::uint64_t>(contract_.element_width);
    static_cast<void>(maximum_value(contract_.element_width));
    if (contract_.instruction_address == 0 || contract_.element_count == 0
        || contract_.maximum_chunk_elements == 0
        || contract_.maximum_chunk_elements > contract_.element_count
        || contract_.source_stride < width
        || contract_.destination_stride < width) {
        throw std::invalid_argument("Memory-transfer contract is empty or overlapping");
    }
    static_cast<void>(checked_address(contract_.source_base, contract_.source_stride,
        contract_.element_count - 1, width, contract_.address_limit_exclusive));
    static_cast<void>(checked_address(contract_.destination_base, contract_.destination_stride,
        contract_.element_count - 1, width, contract_.address_limit_exclusive));
    effects_.reserve(contract_.element_count);
}

BoundedMemoryTransferObservationResult BoundedMemoryTransferSession::observe_chunk(
    const BoundedMemoryTransferChunkObservation& observation) {
    if (next_index_ == contract_.element_count) {
        return {false, "Memory-transfer observation follows completion"};
    }
    if (observation.sequence == 0 || (last_sequence_ && observation.sequence <= *last_sequence_)) {
        return {false, "Memory-transfer chunk sequence is not strictly increasing"};
    }
    if (observation.instruction_address != contract_.instruction_address) {
        return {false, "Memory-transfer chunk instruction does not match its contract"};
    }
    if (observation.values.empty() || observation.first_index != next_index_
        || observation.values.size() > contract_.maximum_chunk_elements
        || observation.values.size() > contract_.element_count - next_index_) {
        return {false, "Memory-transfer chunk is empty, duplicated, gapped, or oversized"};
    }
    const auto width = static_cast<std::uint64_t>(contract_.element_width);
    const auto expected_source = checked_address(contract_.source_base,
        contract_.source_stride, next_index_, width, contract_.address_limit_exclusive);
    const auto expected_destination = checked_address(contract_.destination_base,
        contract_.destination_stride, next_index_, width, contract_.address_limit_exclusive);
    if (observation.source_address != expected_source
        || observation.destination_address != expected_destination) {
        return {false, "Memory-transfer chunk addresses do not match the exact next element"};
    }
    const auto maximum = maximum_value(contract_.element_width);
    for (const auto value : observation.values) {
        if (value > maximum) {
            return {false, "Memory-transfer value exceeds its declared element width"};
        }
    }

    for (const auto value : observation.values) {
        effects_.push_back({next_index_,
            checked_address(contract_.source_base, contract_.source_stride, next_index_, width,
                contract_.address_limit_exclusive),
            checked_address(contract_.destination_base, contract_.destination_stride, next_index_, width,
                contract_.address_limit_exclusive), value});
        ++next_index_;
    }
    last_sequence_ = observation.sequence;
    return {true, {}};
}

BoundedMemoryTransferCheckpoint BoundedMemoryTransferSession::checkpoint() const {
    return {contract_, next_index_, last_sequence_, next_index_ == contract_.element_count, effects_};
}

} // namespace eon
