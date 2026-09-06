#pragma once

#include "engine/native_runtime_memory.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace eon {

// Immutable renderer DTO for a caller-connected recurring main-stage frame.
// Pixels come only from owned runtime memory; the palette is copied only from
// an already accepted pair of original LoadRGB4 calls.
struct DeuterosAmigaMainStageFrameSnapshot {
    std::uint16_t width = 320;
    std::uint16_t height = 200;
    std::uint64_t generation = 0;
    std::uint32_t plane_base = 0;
    std::uint16_t frame_counter = 0;
    std::uint64_t runtime_memory_checksum = 0;
    std::array<std::uint16_t, 16> palette_rgb4{};
    std::vector<std::uint8_t> color_indices;
    std::vector<std::uint8_t> rgba;
    std::string planar_sha256;
    std::string color_indices_sha256;
    std::string rgba_sha256;
    bool operator==(const DeuterosAmigaMainStageFrameSnapshot&) const = default;
};

using DeuterosAmigaMainStageFrame =
    std::shared_ptr<const DeuterosAmigaMainStageFrameSnapshot>;

[[nodiscard]] std::optional<DeuterosAmigaMainStageFrameSnapshot>
decode_deuteros_amiga_main_stage_frame(
    const NativeRuntimeMemoryCheckpoint& memory,
    std::uint32_t plane_base,
    std::uint16_t frame_counter,
    const std::array<std::uint16_t, 16>& palette_rgb4,
    std::uint64_t generation);

[[nodiscard]] std::optional<DeuterosAmigaMainStageFrameSnapshot>
recolor_deuteros_amiga_main_stage_frame(
    const DeuterosAmigaMainStageFrameSnapshot& frame,
    const std::array<std::uint16_t, 16>& palette_rgb4,
    std::uint64_t generation);

} // namespace eon
