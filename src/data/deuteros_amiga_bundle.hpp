#pragma once

#include "data/amiga_adf.hpp"

#include <array>
#include <cstdint>

namespace eon {

struct DeuterosAmigaBundle {
    std::uint32_t disk_offset = 0;
    std::uint32_t length = 0;
    std::uint16_t object_count = 0;
    std::array<std::uint32_t, 7> channel_offsets{};
    std::array<std::uint32_t, 6> auxiliary_offsets{};
    std::uint16_t mode_flag = 0;
};

// Parse the in-memory pointer catalogue used by the original 68000 program.
// Offsets remain relative to the bundle, just as they are stored on disk.
[[nodiscard]] DeuterosAmigaBundle parse_deuteros_amiga_bundle(
    const AmigaAdf& disk, std::uint32_t disk_offset);

} // namespace eon
