#include "engine/deuteros_amiga_bootstrap_frame.hpp"

#include "data/sha256.hpp"

#include <map>

namespace eon {
namespace {

constexpr std::uint32_t palette_address = 0x20000;
constexpr std::uint32_t plane_size = 0x1f40;
constexpr std::uint32_t bytes_per_row = 40;
constexpr std::uint32_t height = 200;
constexpr std::string_view expected_indices_sha256 =
    "a55891e61536aa9d154720f08858b66ca7754b5b72d0bd94578df16ad5fd7e39";
constexpr std::string_view expected_rgba_sha256 =
    "9d3a40c805ada5111f9253ef1a06e5f6d0f6a523b8920485b22ea8864e852f65";

} // namespace

std::optional<DeuterosAmigaBootstrapFrameSnapshot>
decode_deuteros_amiga_bootstrap_frame(const NativeRuntimeMemoryCheckpoint& memory,
    const std::uint32_t plane_base, const std::uint64_t generation) {
    std::map<std::uint32_t, std::uint8_t> bytes;
    for (const auto& cell : memory.initialized_bytes) {
        if (cell.location.address_space != NativeRuntimeAddressSpace::linear
            || cell.location.segment || cell.location.offset > 0xffffffffULL) continue;
        const auto address = static_cast<std::uint32_t>(cell.location.offset);
        if (!bytes.emplace(address, cell.value).second) return std::nullopt;
    }
    const auto read_byte = [&bytes](const std::uint32_t address) -> std::optional<std::uint8_t> {
        const auto found = bytes.find(address);
        if (found == bytes.end()) return std::nullopt;
        return found->second;
    };
    if (generation == 0 || (plane_base & 1U) != 0
        || plane_base > 0x1000000U - 4U * plane_size) {
        return std::nullopt;
    }

    DeuterosAmigaBootstrapFrameSnapshot result;
    result.generation = generation;
    result.plane_base = plane_base;
    result.runtime_memory_checksum = memory.checksum;
    for (std::uint32_t color = 0; color < result.palette_rgb4.size(); ++color) {
        const auto high = read_byte(palette_address + color * 2U);
        const auto low = read_byte(palette_address + color * 2U + 1U);
        if (!high || !low) return std::nullopt;
        const auto rgb4 = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(*high) << 8U) | *low);
        if ((rgb4 & 0xf000U) != 0) return std::nullopt;
        result.palette_rgb4[color] = rgb4;
    }

    result.color_indices.reserve(320U * height);
    result.rgba.reserve(320U * height * 4U);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x_byte = 0; x_byte < bytes_per_row; ++x_byte) {
            std::array<std::uint8_t, 4> planes{};
            for (std::uint32_t plane = 0; plane < planes.size(); ++plane) {
                const auto value = read_byte(plane_base + plane * plane_size
                    + y * bytes_per_row + x_byte);
                if (!value) return std::nullopt;
                planes[plane] = *value;
            }
            for (std::uint32_t bit = 0; bit < 8; ++bit) {
                std::uint8_t index = 0;
                for (std::uint32_t plane = 0; plane < planes.size(); ++plane) {
                    index = static_cast<std::uint8_t>(index
                        | (((planes[plane] >> (7U - bit)) & 1U) << plane));
                }
                result.color_indices.push_back(index);
                const auto rgb4 = result.palette_rgb4[index];
                result.rgba.insert(result.rgba.end(), {
                    static_cast<std::uint8_t>(((rgb4 >> 8U) & 0xfU) * 17U),
                    static_cast<std::uint8_t>(((rgb4 >> 4U) & 0xfU) * 17U),
                    static_cast<std::uint8_t>((rgb4 & 0xfU) * 17U), 0xff});
            }
        }
    }
    result.color_indices_sha256 = to_hex(sha256(result.color_indices));
    result.rgba_sha256 = to_hex(sha256(result.rgba));
    if (result.color_indices_sha256 != expected_indices_sha256
        || result.rgba_sha256 != expected_rgba_sha256) return std::nullopt;
    return result;
}

} // namespace eon
