#pragma once

#include "engine/native_runtime_memory.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace eon {

struct MillenniumDosNativeFileReadResult {
    bool accepted = false;
    std::string error;
    std::size_t committed_byte_count = 0;
};

// Commit only a complete DOS read whose bytes were borrowed from the active,
// hash-verified release media. This never writes original media to disk.
[[nodiscard]] MillenniumDosNativeFileReadResult
commit_millennium_dos_native_file_read(
    NativeRuntimeMemory& memory, std::uint64_t generation, std::uint64_t sequence,
    std::uint16_t destination_segment, std::uint16_t destination_offset,
    std::uint16_t requested_byte_count, std::uint16_t returned_byte_count,
    bool carry_set, std::span<const std::uint8_t> original_bytes);

} // namespace eon
