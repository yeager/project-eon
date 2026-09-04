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

DeuterosAmigaTitlePlanarSurface::ApplyResult
DeuterosAmigaTitlePlanarSurface::apply(
    const std::span<const std::uint32_t, 32> destination_addresses,
    const std::span<const std::uint8_t, 32> destination_values,
    const std::uint64_t command_generation) {
    if (command_generation == 0 || command_generation <= last_command_generation_) {
        return {false, "Planar surface command generation is stale"};
    }
    const auto base = destination_addresses.front();
    if (base < display_plane_zero || base - display_plane_zero >= plane_stride) {
        return {false, "Planar surface destination is outside plane zero"};
    }
    const auto base_offset = base - display_plane_zero;
    const auto row = base_offset / bytes_per_row;
    if (row > height_lines - 8U) {
        return {false, "Planar surface patch crosses the recovered display height"};
    }
    std::array<std::size_t, 32> offsets{};
    for (std::uint32_t patch_row = 0; patch_row < 8U; ++patch_row) {
        for (std::uint32_t plane = 0; plane < 4U; ++plane) {
            const auto index = patch_row * 4U + plane;
            const auto expected = base + patch_row * bytes_per_row + plane * plane_stride;
            if (destination_addresses[index] != expected) {
                return {false, "Planar surface effect order does not match the recovered layout"};
            }
            offsets[index] = static_cast<std::size_t>(expected - display_plane_zero);
            if (offsets[index] >= plane_bytes_.size()) {
                return {false, "Planar surface effect exceeds the recovered layout"};
            }
        }
    }

    // Validation above is complete, so this commit cannot partially fail.
    for (std::size_t index = 0; index < offsets.size(); ++index) {
        const auto offset = offsets[index];
        plane_bytes_[offset] = destination_values[index];
        if (!initialized_[offset]) {
            initialized_[offset] = 1;
            ++initialized_plane_byte_count_;
        }
    }
    last_command_generation_ = command_generation;
    ++applied_patch_count_;
    return {true, {}};
}

std::optional<DeuterosAmigaTitlePlanarSurfaceSnapshot>
DeuterosAmigaTitlePlanarSurface::snapshot(
    const std::span<const RgbColor, 16> palette,
    const std::uint64_t runtime_memory_checksum) const {
    if (applied_patch_count_ == 0 || last_command_generation_ == 0) return std::nullopt;

    DeuterosAmigaTitlePlanarSurfaceSnapshot result;
    result.last_command_generation = last_command_generation_;
    result.applied_patch_count = applied_patch_count_;
    result.initialized_plane_byte_count = initialized_plane_byte_count_;
    result.runtime_memory_checksum = runtime_memory_checksum;
    result.display_trace_bitplanes_sha256 = std::string(display_trace_bitplanes_sha256);
    result.palette_rgb4_sha256 = std::string(palette_sha256);
    constexpr std::size_t pixel_count = bytes_per_row * 8U * height_lines;
    result.valid_pixels.assign(pixel_count, 0);
    result.color_indices.assign(pixel_count, 0);
    result.rgba.assign(pixel_count * 4U, 0);

    for (std::uint32_t y = 0; y < height_lines; ++y) {
        for (std::uint32_t x_byte = 0; x_byte < bytes_per_row; ++x_byte) {
            const auto byte_offset = static_cast<std::size_t>(y * bytes_per_row + x_byte);
            std::array<std::uint8_t, 4> planes{};
            bool complete = true;
            for (std::uint32_t plane = 0; plane < planes.size(); ++plane) {
                const auto offset = byte_offset + plane * plane_stride;
                if (!initialized_[offset]) {
                    complete = false;
                    break;
                }
                planes[plane] = plane_bytes_[offset];
            }
            if (!complete) continue;
            for (std::uint32_t bit = 0; bit < 8U; ++bit) {
                std::uint8_t color = 0;
                for (std::uint32_t plane = 0; plane < planes.size(); ++plane) {
                    color = static_cast<std::uint8_t>(color
                        | (((planes[plane] >> (7U - bit)) & 1U) << plane));
                }
                const auto pixel = static_cast<std::size_t>(
                    y * bytes_per_row * 8U + x_byte * 8U + bit);
                result.valid_pixels[pixel] = 1;
                result.color_indices[pixel] = color;
                result.rgba[pixel * 4U] = palette[color].red;
                result.rgba[pixel * 4U + 1U] = palette[color].green;
                result.rgba[pixel * 4U + 2U] = palette[color].blue;
                result.rgba[pixel * 4U + 3U] = 0xff;
                ++result.decoded_pixel_count;
            }
        }
    }
    return result;
}

} // namespace eon
