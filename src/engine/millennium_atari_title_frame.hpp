#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace eon {

struct MillenniumAtariTitleFrameSnapshot {
    std::uint16_t width = 320;
    std::uint16_t height = 200;
    std::uint64_t generation = 0;
    std::array<std::uint16_t, 16> palette_rgb3{};
    std::vector<std::uint8_t> planar_bytes;
    std::vector<std::uint8_t> color_indices;
    std::vector<std::uint8_t> rgba;
    std::string planar_sha256;
    std::string color_indices_sha256;
    std::string palette_sha256;
    std::string rgba_sha256;
    bool operator==(const MillenniumAtariTitleFrameSnapshot&) const = default;
};

// Decodes the hash-identified Equinox C file into the original low-resolution
// four-plane title and replays its seven source-verified RGB3 palette passes.
[[nodiscard]] MillenniumAtariTitleFrameSnapshot decode_millennium_atari_title_frame(
    std::span<const std::uint8_t> mill22a, std::span<const std::uint8_t> mill22c,
    std::uint64_t generation);

} // namespace eon
