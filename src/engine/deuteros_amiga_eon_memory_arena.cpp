#include "engine/deuteros_amiga_eon_memory_arena.hpp"

#include <limits>
#include <stdexcept>

namespace eon {

DeuterosAmigaEonMemoryArena::DeuterosAmigaEonMemoryArena(
    const std::uint64_t runtime_generation)
    : runtime_generation_(runtime_generation), arena_(chip_capacity_bytes, 0) {
    if (runtime_generation_ == 0
        || runtime_generation_ > 0x0fffffffULL
        || arena_.size() != chip_capacity_bytes) {
        throw std::runtime_error("Invalid Deuteros Eon-native memory arena");
    }
}

std::size_t DeuterosAmigaEonMemoryArena::sync_and_count(
    const NativeRuntimeMemory& memory) {
    std::vector<bool> occupied(arena_.size(), false);
    std::size_t occupied_count = 0;
    const auto checkpoint = memory.checkpoint();
    for (const auto& cell : checkpoint.initialized_bytes) {
        if (cell.location.address_space != NativeRuntimeAddressSpace::linear
            || cell.location.segment || cell.location.offset >= arena_.size()) {
            continue;
        }
        const auto address = static_cast<std::size_t>(cell.location.offset);
        arena_[address] = cell.value;
        if (!occupied[address]) {
            occupied[address] = true;
            ++occupied_count;
        }
    }
    return occupied_count;
}

std::optional<DeuterosAmigaEonAvailMemReceipt>
DeuterosAmigaEonMemoryArena::avail_mem(const NativeRuntimeMemory& memory,
    const std::uint32_t call_address, const std::uint32_t return_address,
    const std::uint32_t memory_flags) {
    if (runtime_generation_ == 0 || next_service_sequence_ > 2) return std::nullopt;

    const bool first = next_service_sequence_ == 1;
    if (first) {
        if (first_query_complete_ || call_address != first_call_address
            || return_address != 0x1302c || memory_flags != fast_memory_flag) {
            return std::nullopt;
        }
    } else if (!first_query_complete_ || second_query_complete_
        || call_address != second_call_address || return_address != 0x13038
        || memory_flags != chip_memory_flag) {
        return std::nullopt;
    }

    const auto occupied = sync_and_count(memory);
    const auto free_bytes = first ? 0U
        : static_cast<std::uint32_t>(arena_.size() - occupied);
    if (!first && (occupied == 0 || occupied >= arena_.size()
            || free_bytes == 0 || free_bytes >= chip_capacity_bytes)) {
        return std::nullopt;
    }

    const auto status = free_bytes == 0 ? static_cast<std::uint16_t>(0x0004U)
        : static_cast<std::uint16_t>(0);
    DeuterosAmigaEonAvailMemReceipt receipt{
        runtime_generation_, next_service_sequence_, call_address,
        return_address, memory_flags, free_bytes, status,
        arena_.size(), occupied};
    if (first) first_query_complete_ = true;
    else second_query_complete_ = true;
    ++next_service_sequence_;
    return receipt;
}

std::optional<NativeRuntimeEffectBatch>
DeuterosAmigaEonMemoryArena::reserve_selected_planes(
    const NativeRuntimeMemory& memory, const std::uint32_t address,
    const std::size_t byte_count, std::string batch_id) {
    if (runtime_generation_ == 0 || !second_query_complete_ || plane_reserved_
        || address != selected_plane_address || byte_count != four_plane_bytes
        || address > arena_.size() || byte_count > arena_.size() - address
        || batch_id.empty()) {
        return std::nullopt;
    }
    static_cast<void>(sync_and_count(memory));
    for (std::size_t offset = 0; offset < byte_count; ++offset) {
        if (memory.read_byte({NativeRuntimeAddressSpace::linear, std::nullopt,
                static_cast<std::uint64_t>(address) + offset})) {
            return std::nullopt;
        }
    }

    NativeRuntimeEffectBatch batch{std::move(batch_id), true, {}};
    batch.effects.reserve(byte_count);
    for (std::size_t offset = 0; offset < byte_count; ++offset) {
        arena_[address + offset] = 0;
        batch.effects.push_back({offset + 1,
            {NativeRuntimeAddressSpace::linear, std::nullopt, address + offset},
            MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian, 0});
    }
    plane_reserved_ = true;
    return batch;
}

} // namespace eon
