#pragma once

#include "engine/native_runtime_memory.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace eon {

// Complete renderer-facing image produced by the caller-proven bootstrap
// auxiliary transfer. All pixels and colours are decoded from initialized
// native runtime bytes; this snapshot does not infer graphics.library state.
struct DeuterosAmigaBootstrapFrameSnapshot {
    std::uint16_t width = 320;
    std::uint16_t height = 200;
    std::uint64_t generation = 0;
    std::uint32_t plane_base = 0;
    std::uint64_t runtime_memory_checksum = 0;
    std::array<std::uint16_t, 16> palette_rgb4{};
    std::vector<std::uint8_t> color_indices;
    std::vector<std::uint8_t> rgba;
    std::string color_indices_sha256;
    std::string rgba_sha256;
    bool operator==(const DeuterosAmigaBootstrapFrameSnapshot&) const = default;
};

// Decodes the exact four 8,000-byte 320x200 bitplanes at the caller-validated
// dynamic plane base and the RGB4 palette staged at $20000. The function
// fails closed unless every source byte is initialized and the resulting
// genuine-media frame matches its preservation hashes.
[[nodiscard]] std::optional<DeuterosAmigaBootstrapFrameSnapshot>
decode_deuteros_amiga_bootstrap_frame(const NativeRuntimeMemoryCheckpoint& memory,
    std::uint32_t plane_base, std::uint64_t generation);

} // namespace eon
