#pragma once

#include "engine/bounded_memory_transfer.hpp"

#include <cstddef>
#include <compare>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace eon {

struct MillenniumDosBdfCheckpoint;

enum class NativeRuntimeAddressSpace : std::uint8_t { linear, dos_segmented };
enum class NativeRuntimeByteOrder : std::uint8_t { little_endian, big_endian };

struct NativeRuntimeLocation {
    NativeRuntimeAddressSpace address_space = NativeRuntimeAddressSpace::linear;
    std::optional<std::uint16_t> segment;
    std::uint64_t offset = 0;
    constexpr bool operator==(const NativeRuntimeLocation&) const = default;
    constexpr auto operator<=>(const NativeRuntimeLocation&) const = default;
};

struct NativeRuntimeWriteEffect {
    std::size_t order = 0;
    NativeRuntimeLocation location;
    MemoryTransferElementWidth width = MemoryTransferElementWidth::byte;
    NativeRuntimeByteOrder byte_order = NativeRuntimeByteOrder::little_endian;
    std::uint32_t value = 0;
};

struct NativeRuntimeEffectBatch {
    std::string id;
    bool fully_admitted = false;
    std::vector<NativeRuntimeWriteEffect> effects;
};

struct NativeRuntimeMemoryCell {
    NativeRuntimeLocation location;
    std::uint8_t value = 0;
    constexpr bool operator==(const NativeRuntimeMemoryCell&) const = default;
};

struct NativeRuntimeMemoryCheckpoint {
    std::vector<NativeRuntimeMemoryCell> initialized_bytes;
    std::size_t applied_batch_count = 0;
    std::uint64_t checksum = 0;
};

struct NativeRuntimeMemoryDiagnostics {
    std::size_t initialized_byte_count = 0;
    std::size_t applied_batch_count = 0;
    std::uint64_t checksum = 0;
};

struct NativeRuntimeMemoryApplyResult { bool accepted=false; std::string error; };

class NativeRuntimeMemory {
public:
    NativeRuntimeMemory(std::uint64_t linear_limit_exclusive = 0x100000000ULL,
        std::uint64_t segmented_offset_limit_exclusive = 0x10000ULL);
    [[nodiscard]] NativeRuntimeMemoryApplyResult apply(const NativeRuntimeEffectBatch& batch);
    [[nodiscard]] std::optional<std::uint8_t> read_byte(
        const NativeRuntimeLocation& location) const;
    [[nodiscard]] NativeRuntimeMemoryCheckpoint checkpoint() const;
    [[nodiscard]] NativeRuntimeMemoryDiagnostics diagnostics() const;

private:
    std::uint64_t linear_limit_exclusive_;
    std::uint64_t segmented_offset_limit_exclusive_;
    std::map<NativeRuntimeLocation,std::uint8_t> bytes_;
    std::set<std::string> applied_batch_ids_;
};

[[nodiscard]] std::optional<NativeRuntimeEffectBatch> make_bounded_memory_transfer_batch(
    const BoundedMemoryTransferCheckpoint& checkpoint, std::string id);
[[nodiscard]] std::optional<NativeRuntimeEffectBatch> make_millennium_dos_bdf_effect_batch(
    const MillenniumDosBdfCheckpoint& checkpoint, std::string id);

} // namespace eon
