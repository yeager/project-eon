#include "engine/native_runtime_memory.hpp"
#include "engine/release_runtime.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace eon {
namespace {

[[nodiscard]] std::uint64_t width_bytes(const MemoryTransferElementWidth width) {
    switch (width) {
    case MemoryTransferElementWidth::byte: return 1;
    case MemoryTransferElementWidth::word: return 2;
    case MemoryTransferElementWidth::longword: return 4;
    }
    return 0;
}

[[nodiscard]] std::uint32_t width_maximum(const MemoryTransferElementWidth width) {
    switch (width) {
    case MemoryTransferElementWidth::byte: return 0xffU;
    case MemoryTransferElementWidth::word: return 0xffffU;
    case MemoryTransferElementWidth::longword: return 0xffffffffU;
    }
    return 0;
}

[[nodiscard]] std::uint64_t state_checksum(
    const std::map<NativeRuntimeLocation,std::uint8_t>& bytes) {
    std::uint64_t hash = 1469598103934665603ULL;
    const auto mix = [&hash](const std::uint8_t value) {
        hash ^= value;
        hash *= 1099511628211ULL;
    };
    for (const auto& [location,value] : bytes) {
        mix(static_cast<std::uint8_t>(location.address_space));
        mix(location.segment ? 1U : 0U);
        if (location.segment) {
            mix(static_cast<std::uint8_t>(*location.segment >> 8U));
            mix(static_cast<std::uint8_t>(*location.segment));
        }
        for (unsigned shift=0;shift<64;shift+=8) {
            mix(static_cast<std::uint8_t>(location.offset >> shift));
        }
        mix(value);
    }
    return hash;
}

} // namespace

NativeRuntimeMemory::NativeRuntimeMemory(const std::uint64_t linear_limit_exclusive,
    const std::uint64_t segmented_offset_limit_exclusive)
    : linear_limit_exclusive_(linear_limit_exclusive),
      segmented_offset_limit_exclusive_(segmented_offset_limit_exclusive) {
    if (linear_limit_exclusive_ == 0 || segmented_offset_limit_exclusive_ == 0) {
        throw std::invalid_argument("Runtime memory requires finite nonzero address limits");
    }
}

NativeRuntimeMemoryApplyResult NativeRuntimeMemory::apply(const NativeRuntimeEffectBatch& batch) {
    if (!batch.fully_admitted || batch.id.empty() || batch.effects.empty()) {
        return {false,"Runtime memory accepts only a named, complete admitted effect batch"};
    }
    if (applied_batch_ids_.contains(batch.id)) {
        return {false,"Runtime memory effect batch was already applied"};
    }

    std::map<NativeRuntimeLocation,std::uint8_t> pending;
    for (std::size_t index=0; index<batch.effects.size(); ++index) {
        const auto& effect=batch.effects[index];
        if (effect.order != index+1) {
            return {false,"Runtime memory effects are duplicated, gapped, or reordered"};
        }
        const auto width=width_bytes(effect.width);
        if (width==0 || effect.value>width_maximum(effect.width)) {
            return {false,"Runtime memory effect value exceeds its explicit width"};
        }
        const auto limit=effect.location.address_space==NativeRuntimeAddressSpace::linear
            ? linear_limit_exclusive_ : segmented_offset_limit_exclusive_;
        if ((effect.location.address_space==NativeRuntimeAddressSpace::dos_segmented)
                != effect.location.segment.has_value()
            || effect.location.offset > limit || width > limit-effect.location.offset) {
            return {false,"Runtime memory effect location is invalid or out of bounds"};
        }
        for (std::uint64_t byte=0;byte<width;++byte) {
            auto location=effect.location;
            location.offset+=byte;
            const auto shift=effect.byte_order==NativeRuntimeByteOrder::little_endian
                ? static_cast<unsigned>(byte*8U)
                : static_cast<unsigned>((width-1U-byte)*8U);
            if (!pending.emplace(location,static_cast<std::uint8_t>(effect.value>>shift)).second) {
                return {false,"Runtime memory effects overlap within one admitted batch"};
            }
        }
    }

    // Commit through complete copies so allocation failure cannot leave a
    // partially applied batch or a byte state detached from its batch ID.
    auto next_bytes=bytes_;
    auto next_batch_ids=applied_batch_ids_;
    for (const auto& [location,value] : pending) next_bytes[location]=value;
    next_batch_ids.insert(batch.id);
    bytes_.swap(next_bytes);
    applied_batch_ids_.swap(next_batch_ids);
    return {true,{}};
}

std::optional<std::uint8_t> NativeRuntimeMemory::read_byte(
    const NativeRuntimeLocation& location) const {
    const auto found=bytes_.find(location);
    if (found==bytes_.end()) return std::nullopt;
    return found->second;
}

NativeRuntimeMemoryCheckpoint NativeRuntimeMemory::checkpoint() const {
    NativeRuntimeMemoryCheckpoint result;
    result.applied_batch_count=applied_batch_ids_.size();
    result.checksum=state_checksum(bytes_);
    result.initialized_bytes.reserve(bytes_.size());
    for (const auto& [location,value] : bytes_) result.initialized_bytes.push_back({location,value});
    return result;
}

NativeRuntimeMemoryDiagnostics NativeRuntimeMemory::diagnostics() const {
    return {bytes_.size(),applied_batch_ids_.size(),state_checksum(bytes_)};
}

std::optional<NativeRuntimeEffectBatch> make_bounded_memory_transfer_batch(
    const BoundedMemoryTransferCheckpoint& checkpoint, std::string id) {
    if (!checkpoint.complete || checkpoint.next_index!=checkpoint.contract.element_count
        || checkpoint.effects.size()!=checkpoint.contract.element_count || id.empty()) {
        return std::nullopt;
    }
    NativeRuntimeEffectBatch batch{std::move(id),true,{}};
    batch.effects.reserve(checkpoint.effects.size());
    for (const auto& effect : checkpoint.effects) {
        if (effect.index+1 != batch.effects.size()+1) return std::nullopt;
        batch.effects.push_back({effect.index+1,
            {NativeRuntimeAddressSpace::linear,std::nullopt,effect.destination_address},
            checkpoint.contract.element_width,NativeRuntimeByteOrder::big_endian,effect.value});
    }
    return batch;
}

std::optional<NativeRuntimeEffectBatch> make_millennium_dos_bdf_effect_batch(
    const MillenniumDosBdfCheckpoint& checkpoint, std::string id) {
    if (id.empty() || !checkpoint.terminal_transfer
        || !checkpoint.terminal_transfer->returned || !checkpoint.mode_two
        || checkpoint.mode_two->state!=MillenniumDosBdfModeTwoState::returned) {
        return std::nullopt;
    }
    const auto& mode=*checkpoint.mode_two;
    if (mode.far_effects.empty()==mode.far_byte_effects.empty()) return std::nullopt;
    NativeRuntimeEffectBatch batch{std::move(id),true,{}};
    batch.effects.reserve(mode.far_effects.size()+mode.far_byte_effects.size());
    for (const auto& effect : mode.far_effects) {
        batch.effects.push_back({batch.effects.size()+1,
            {NativeRuntimeAddressSpace::dos_segmented,effect.segment,effect.offset},
            MemoryTransferElementWidth::word,NativeRuntimeByteOrder::little_endian,effect.value});
    }
    for (const auto& effect : mode.far_byte_effects) {
        batch.effects.push_back({batch.effects.size()+1,
            {NativeRuntimeAddressSpace::dos_segmented,effect.segment,effect.offset},
            MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::little_endian,effect.value});
    }
    return batch;
}

} // namespace eon
