#include "engine/deuteros_amiga_main_stage_frame.hpp"

#include "data/sha256.hpp"

#include <algorithm>

namespace eon {

namespace {

bool append_rgba(const std::vector<std::uint8_t>& color_indices,
    const std::array<std::uint16_t, 16>& palette_rgb4,
    std::vector<std::uint8_t>& rgba) {
    for (const auto color : palette_rgb4)
        if ((color & 0xf000U) != 0) return false;
    rgba.clear();
    rgba.reserve(color_indices.size() * 4U);
    for (const auto index : color_indices) {
        if (index >= palette_rgb4.size()) return false;
        const auto rgb4 = palette_rgb4[index];
        rgba.insert(rgba.end(), {
            static_cast<std::uint8_t>(((rgb4 >> 8U) & 0xfU) * 17U),
            static_cast<std::uint8_t>(((rgb4 >> 4U) & 0xfU) * 17U),
            static_cast<std::uint8_t>((rgb4 & 0xfU) * 17U), 0xff});
    }
    return true;
}

} // namespace

std::optional<DeuterosAmigaMainStageFrameSnapshot>
decode_deuteros_amiga_main_stage_frame(
    const NativeRuntimeMemoryCheckpoint& memory,
    const std::uint32_t plane_base,
    const std::uint16_t frame_counter,
    const std::array<std::uint16_t, 16>& palette_rgb4,
    const std::uint64_t generation) {
    constexpr std::uint32_t plane_size = 8000;
    constexpr std::uint32_t bytes_per_row = 40;
    constexpr std::uint32_t height = 200;
    if (generation == 0 || (plane_base & 1U) != 0
        || plane_base > 0x1000000U - 4U * plane_size) return std::nullopt;
    for (const auto color : palette_rgb4)
        if ((color & 0xf000U) != 0) return std::nullopt;

    std::vector<std::uint8_t> planar(4U * plane_size);
    std::vector<bool> initialized(planar.size(), false);
    for (const auto& cell : memory.initialized_bytes) {
        if (cell.location.address_space != NativeRuntimeAddressSpace::linear
            || cell.location.segment || cell.location.offset > 0xffffffffULL) continue;
        const auto address = static_cast<std::uint32_t>(cell.location.offset);
        if (address < plane_base || address >= plane_base + planar.size()) continue;
        const auto index = static_cast<std::size_t>(address - plane_base);
        if (initialized[index]) return std::nullopt;
        planar[index] = cell.value;
        initialized[index] = true;
    }
    if (!std::all_of(initialized.begin(), initialized.end(), [](const bool value) {
            return value;
        })) return std::nullopt;

    DeuterosAmigaMainStageFrameSnapshot result;
    result.generation = generation;
    result.plane_base = plane_base;
    result.frame_counter = frame_counter;
    result.runtime_memory_checksum = memory.checksum;
    result.palette_rgb4 = palette_rgb4;
    result.color_indices.reserve(320U * height);
    result.rgba.reserve(320U * height * 4U);
    for (std::uint32_t y = 0; y < height; ++y)
        for (std::uint32_t x_byte = 0; x_byte < bytes_per_row; ++x_byte) {
            std::array<std::uint8_t, 4> planes{};
            for (std::uint32_t plane = 0; plane < 4; ++plane)
                planes[plane] = planar[plane * plane_size + y * bytes_per_row + x_byte];
            for (std::uint32_t bit = 0; bit < 8; ++bit) {
                std::uint8_t index = 0;
                for (std::uint32_t plane = 0; plane < 4; ++plane)
                    index = static_cast<std::uint8_t>(index
                        | (((planes[plane] >> (7U - bit)) & 1U) << plane));
                result.color_indices.push_back(index);
            }
        }
    if (!append_rgba(result.color_indices, palette_rgb4, result.rgba)) return std::nullopt;
    result.planar_sha256 = to_hex(sha256(planar));
    result.color_indices_sha256 = to_hex(sha256(result.color_indices));
    result.rgba_sha256 = to_hex(sha256(result.rgba));
    return result;
}

std::optional<DeuterosAmigaMainStageFrameSnapshot>
recolor_deuteros_amiga_main_stage_frame(
    const DeuterosAmigaMainStageFrameSnapshot& frame,
    const std::array<std::uint16_t, 16>& palette_rgb4,
    const std::uint64_t generation) {
    if (generation == 0 || frame.width != 320 || frame.height != 200
        || frame.color_indices.size() != std::size_t(frame.width) * frame.height) {
        return std::nullopt;
    }
    auto result = frame;
    result.generation = generation;
    result.palette_rgb4 = palette_rgb4;
    if (!append_rgba(result.color_indices, palette_rgb4, result.rgba)) return std::nullopt;
    result.rgba_sha256 = to_hex(sha256(result.rgba));
    return result;
}

} // namespace eon
