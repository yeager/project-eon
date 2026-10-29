#include "engine/millennium_dos_file_read_session.hpp"

#include <limits>
#include <vector>

namespace eon {

MillenniumDosNativeFileReadResult commit_millennium_dos_native_file_read(
    NativeRuntimeMemory& memory, const std::uint64_t generation,
    const std::uint64_t sequence, const std::uint16_t destination_segment,
    const std::uint16_t destination_offset, const std::uint16_t requested_byte_count,
    const std::uint16_t returned_byte_count, const bool carry_set,
    const std::span<const std::uint8_t> original_bytes) {
    MillenniumDosNativeFileReadResult result;
    if (generation == 0 || sequence == 0 || destination_segment == 0
        || requested_byte_count == 0 || carry_set) {
        result.error = "Millennium DOS file read is detached from a successful owned request";
        return result;
    }
    if (original_bytes.empty() || original_bytes.size() > requested_byte_count
        || original_bytes.size() != returned_byte_count) {
        result.error = "Millennium DOS file read count differs from its original media leaf";
        return result;
    }
    if (static_cast<std::size_t>(destination_offset) + original_bytes.size()
        > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()) + 1U) {
        result.error = "Millennium DOS file read exceeds its segmented destination";
        return result;
    }

    std::vector<NativeRuntimeWriteEffect> effects;
    effects.reserve(original_bytes.size());
    std::size_t ordinal = 0;
    for (std::size_t index = 0; index < original_bytes.size(); ++index) {
        const auto offset = static_cast<std::uint16_t>(destination_offset + index);
        const NativeRuntimeLocation location{NativeRuntimeAddressSpace::dos_segmented,
            destination_segment, offset};
        const auto existing = memory.read_byte(location);
        if (existing) {
            if (*existing != original_bytes[index]) {
                result.error = "Millennium DOS file read contradicts existing owned destination bytes";
                return result;
            }
            continue;
        }
        effects.push_back({++ordinal, location, MemoryTransferElementWidth::byte,
            NativeRuntimeByteOrder::little_endian, original_bytes[index]});
    }

    if (!effects.empty()) {
        auto next = memory;
        const auto applied = next.apply({
            "millennium-dos-file-read-" + std::to_string(generation)
                + "-" + std::to_string(sequence), true, std::move(effects)});
        if (!applied.accepted) {
            result.error = applied.error;
            return result;
        }
        memory = std::move(next);
    }
    result.accepted = true;
    result.committed_byte_count = original_bytes.size();
    return result;
}

} // namespace eon
