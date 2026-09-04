#pragma once

#include "data/deuteros_amiga_bundle.hpp"
#include "engine/native_runtime_memory.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace eon {

// Renderer-facing immutable result for one fully admitted original 8x8
// planar write. It is intentionally a patch, not a fabricated full frame:
// pixels outside these 64 positions are not initialized by this command.
struct DeuterosAmigaTitlePlanarPatchSnapshot {
    std::uint64_t command_generation = 0;
    std::uint32_t plane_zero_address = 0;
    std::uint16_t pixel_x = 0;
    std::uint16_t pixel_y = 0;
    std::uint16_t width = 8;
    std::uint16_t height = 8;
    std::uint64_t runtime_memory_checksum = 0;
    std::string display_trace_bitplanes_sha256;
    std::string palette_rgb4_sha256;
    std::array<std::uint8_t, 64> color_indices{};
    std::array<std::uint8_t, 64 * 4> rgba{};
    constexpr bool operator==(const DeuterosAmigaTitlePlanarPatchSnapshot&) const = default;
};

// Decode only the captured, hash-admitted 320x200/four-plane title layout.
// Every plane byte must already exist in NativeRuntimeMemory. Missing bytes,
// an address outside plane zero, or a patch crossing the surface fails closed.
[[nodiscard]] std::optional<DeuterosAmigaTitlePlanarPatchSnapshot>
decode_deuteros_amiga_title_planar_patch(
    const NativeRuntimeMemoryCheckpoint& memory,
    std::uint32_t plane_zero_address,
    std::uint64_t command_generation,
    std::span<const RgbColor, 16> palette);

} // namespace eon
