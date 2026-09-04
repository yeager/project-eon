#pragma once

#include "data/deuteros_amiga_bundle.hpp"
#include "engine/native_runtime_memory.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

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

// Session-owned native backing for the proven 320x200, four-plane title
// layout. Only bytes produced by admitted planar command effects enter this
// surface. Unwritten pixels stay explicitly invalid and transparent; the
// engine never manufactures a background for missing original state.
struct DeuterosAmigaTitlePlanarSurfaceSnapshot {
    std::uint16_t width = 320;
    std::uint16_t height = 200;
    std::uint64_t last_command_generation = 0;
    std::uint64_t applied_patch_count = 0;
    std::uint32_t initialized_plane_byte_count = 0;
    std::uint32_t decoded_pixel_count = 0;
    std::uint64_t runtime_memory_checksum = 0;
    std::string display_trace_bitplanes_sha256;
    std::string palette_rgb4_sha256;
    std::vector<std::uint8_t> valid_pixels;
    std::vector<std::uint8_t> color_indices;
    std::vector<std::uint8_t> rgba;
    bool operator==(const DeuterosAmigaTitlePlanarSurfaceSnapshot&) const = default;
};

class DeuterosAmigaTitlePlanarSurface {
public:
    struct ApplyResult {
        bool accepted = false;
        std::string error;
    };

    // The arrays are the already derived, atomic effects of one hash-gated
    // $20..$8f command. Address order is independently checked here before
    // any surface state changes.
    [[nodiscard]] ApplyResult apply(
        std::span<const std::uint32_t, 32> destination_addresses,
        std::span<const std::uint8_t, 32> destination_values,
        std::uint64_t command_generation);

    [[nodiscard]] std::optional<DeuterosAmigaTitlePlanarSurfaceSnapshot> snapshot(
        std::span<const RgbColor, 16> palette,
        std::uint64_t runtime_memory_checksum) const;

private:
    static constexpr std::size_t plane_byte_count = 4U * 0x1f40U;
    std::array<std::uint8_t, plane_byte_count> plane_bytes_{};
    std::array<std::uint8_t, plane_byte_count> initialized_{};
    std::uint64_t last_command_generation_ = 0;
    std::uint64_t applied_patch_count_ = 0;
    std::uint32_t initialized_plane_byte_count_ = 0;
};

} // namespace eon
