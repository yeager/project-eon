#pragma once

#include "engine/native_runtime_memory.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace eon {

// Eon's explicit Deuteros-native memory profile: one owned 512 KiB chip-like
// arena, with no Fast-memory pool. This is an Eon allocation policy, not an
// emulated Kickstart MemHeader list and not a historical AvailMem result.
struct DeuterosAmigaEonAvailMemReceipt {
    std::uint64_t runtime_generation = 0;
    std::uint64_t service_sequence = 0;
    std::uint32_t call_address = 0;
    std::uint32_t return_address = 0;
    std::uint32_t memory_flags = 0;
    std::uint32_t free_bytes = 0;
    std::uint16_t result_status_register = 0;
    std::size_t arena_capacity_bytes = 0;
    std::size_t occupied_bytes = 0;
};

class DeuterosAmigaEonMemoryArena {
public:
    static constexpr std::size_t chip_capacity_bytes = 0x80000;
    static constexpr std::uint32_t chip_memory_flag = 2;
    static constexpr std::uint32_t fast_memory_flag = 4;
    static constexpr std::uint32_t first_call_address = 0x13028;
    static constexpr std::uint32_t second_call_address = 0x13034;
    static constexpr std::uint32_t selected_plane_address = 0x0000ab00;
    static constexpr std::size_t four_plane_bytes = 0x7d00;

    explicit DeuterosAmigaEonMemoryArena(std::uint64_t runtime_generation);

    [[nodiscard]] std::optional<DeuterosAmigaEonAvailMemReceipt> avail_mem(
        const NativeRuntimeMemory& memory, std::uint32_t call_address,
        std::uint32_t return_address, std::uint32_t memory_flags);
    [[nodiscard]] std::optional<NativeRuntimeEffectBatch> reserve_selected_planes(
        const NativeRuntimeMemory& memory, std::uint32_t address,
        std::size_t byte_count, std::string batch_id);
    [[nodiscard]] std::span<const std::uint8_t> bytes() const noexcept { return arena_; }
    [[nodiscard]] std::uint64_t runtime_generation() const noexcept { return runtime_generation_; }
    [[nodiscard]] bool has_plane_reservation() const noexcept { return plane_reserved_; }

private:
    [[nodiscard]] std::size_t sync_and_count(const NativeRuntimeMemory& memory);

    std::uint64_t runtime_generation_ = 0;
    std::uint64_t next_service_sequence_ = 1;
    std::vector<std::uint8_t> arena_;
    bool first_query_complete_ = false;
    bool second_query_complete_ = false;
    bool plane_reserved_ = false;
};

} // namespace eon
