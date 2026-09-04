#include "engine/deuteros_amiga_title_planar_patch.hpp"

#include <algorithm>
#include <limits>

namespace eon {
namespace {

constexpr std::uint32_t display_plane_zero = 0x0000b5f0;
constexpr std::uint32_t plane_stride = 0x1f40;
constexpr std::uint32_t bytes_per_row = 0x28;
constexpr std::uint32_t height_lines = 0xc8;
constexpr std::string_view display_trace_bitplanes_sha256 =
    "fad588ff5f6e0ec471cb4889987dab4a40c11d7da6e532564d48475149c68490";
constexpr std::string_view palette_sha256 =
    "5903a1c83619d7667c04ac1f3c923dfaa3a1ce0d090d6fd95109616a9b506a55";

std::optional<std::uint8_t> byte_at(
    const NativeRuntimeMemoryCheckpoint& memory, const std::uint32_t address) {
    const auto found = std::find_if(memory.initialized_bytes.begin(),
        memory.initialized_bytes.end(), [address](const NativeRuntimeMemoryCell& cell) {
            return cell.location.address_space == NativeRuntimeAddressSpace::linear
                && !cell.location.segment && cell.location.offset == address;
        });
    if (found == memory.initialized_bytes.end()) return std::nullopt;
    return found->value;
}

} // namespace

std::optional<DeuterosAmigaTitlePlanarPatchSnapshot>
decode_deuteros_amiga_title_planar_patch(
    const NativeRuntimeMemoryCheckpoint& memory,
    const std::uint32_t plane_zero_address,
    const std::uint64_t command_generation,
    const std::span<const RgbColor, 16> palette) {
    constexpr auto plane_size = bytes_per_row * height_lines;
    if (command_generation == 0 || plane_zero_address < display_plane_zero
        || plane_zero_address - display_plane_zero >= plane_size) {
        return std::nullopt;
    }
    const auto offset = plane_zero_address - display_plane_zero;
    const auto x_byte = offset % bytes_per_row;
    const auto y = offset / bytes_per_row;
    if (y > height_lines - 8U || x_byte >= bytes_per_row) return std::nullopt;

    DeuterosAmigaTitlePlanarPatchSnapshot result;
    result.command_generation = command_generation;
    result.plane_zero_address = plane_zero_address;
    result.pixel_x = static_cast<std::uint16_t>(x_byte * 8U);
    result.pixel_y = static_cast<std::uint16_t>(y);
    result.runtime_memory_checksum = memory.checksum;
    result.display_trace_bitplanes_sha256 =
        std::string(display_trace_bitplanes_sha256);
    result.palette_rgb4_sha256 = std::string(palette_sha256);
    for (std::uint32_t row = 0; row < 8U; ++row) {
        std::array<std::uint8_t, 4> planes{};
        for (std::uint32_t plane = 0; plane < planes.size(); ++plane) {
            const auto address = plane_zero_address + row * bytes_per_row
                + plane * plane_stride;
            const auto value = byte_at(memory, address);
            if (!value) return std::nullopt;
            planes[plane] = *value;
        }
        for (std::uint32_t bit = 0; bit < 8U; ++bit) {
            std::uint8_t index = 0;
            for (std::uint32_t plane = 0; plane < planes.size(); ++plane) {
                index = static_cast<std::uint8_t>(index
                    | (((planes[plane] >> (7U - bit)) & 1U) << plane));
            }
            const auto pixel = static_cast<std::size_t>(row * 8U + bit);
            result.color_indices[pixel] = index;
            result.rgba[pixel * 4U] = palette[index].red;
            result.rgba[pixel * 4U + 1U] = palette[index].green;
            result.rgba[pixel * 4U + 2U] = palette[index].blue;
            result.rgba[pixel * 4U + 3U] = 0xff;
        }
    }
    return result;
}

} // namespace eon
