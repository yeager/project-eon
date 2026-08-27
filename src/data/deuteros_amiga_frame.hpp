#pragma once

#include "data/deuteros_amiga_bundle.hpp"
#include "data/deuteros_amiga_channel_vm.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace eon {

struct DeuterosAmigaFrame {
    static constexpr std::uint16_t width = 320;
    static constexpr std::uint16_t height = 200;
    std::vector<std::uint8_t> color_indices;
};

// Compose the channel states in original order. Selector bit 15 chooses the
// masked blit path at $20fb2; unflagged records overwrite all four planes.
[[nodiscard]] DeuterosAmigaFrame compose_deuteros_amiga_frame(
    const AmigaAdf& disk, const DeuterosAmigaBundle& bundle,
    const DeuterosAmigaIndexedBlob& blob,
    const std::vector<DeuterosAmigaChannelState>& channels);

[[nodiscard]] std::vector<std::uint8_t> colorize_deuteros_amiga_frame(
    const DeuterosAmigaFrame& frame, const std::array<RgbColor, 16>& palette);

} // namespace eon
