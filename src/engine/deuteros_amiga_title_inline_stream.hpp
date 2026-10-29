#pragma once

#include "engine/native_runtime_memory.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace eon {

struct DeuterosAmigaTitleInlineStreamBoundary {
    std::uint64_t sequence = 0;
    std::string_view release_sha256;
    std::uint32_t program_counter = 0;
    std::uint32_t stack_pointer = 0;
    std::uint32_t return_address = 0;
};

struct DeuterosAmigaTitleInlineStreamAdvance {
    std::uint64_t sequence = 0;
    std::uint32_t next_program_counter = 0;
    std::uint32_t next_stack_pointer = 0;
    std::uint32_t inline_stream_next_address = 0;
    std::vector<NativeRuntimeEffectBatch> ordered_batches;
};

// Native translation of the exact three-command inline stream reached when
// $40794 calls $1f9a4. It stops after the zero terminator at $407a1, before
// the caller resumes at $407a2. The scheduler does not call this automatically.
[[nodiscard]] std::optional<DeuterosAmigaTitleInlineStreamAdvance>
advance_deuteros_amiga_title_inline_stream(
    const NativeRuntimeMemory& memory,
    std::span<const std::uint8_t> original_title_stage,
    const DeuterosAmigaTitleInlineStreamBoundary& boundary);

} // namespace eon
