#pragma once

#include "data/deuteros_amiga_bundle.hpp"
#include "data/deuteros_amiga_channel_vm.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace eon {

struct DeuterosAmigaFrame {
    static constexpr std::uint16_t width = 320;
    static constexpr std::uint16_t height = 200;
    std::vector<std::uint8_t> color_indices;
};

// $20c8c operates on a persistent four-plane display, not a newly cleared
// bitmap on every scheduler tick.  It owns the single $23024 scratch area
// used by the bit-13 restore and bit-15+14 save paths.
class DeuterosAmigaCompositor {
public:
    DeuterosAmigaCompositor();

    void reset();
    [[nodiscard]] const DeuterosAmigaFrame& compose(
        const AmigaAdf& disk, const DeuterosAmigaBundle& bundle,
        const DeuterosAmigaIndexedBlob& blob,
        std::vector<DeuterosAmigaChannelState>& channels);

    [[nodiscard]] bool has_saved_scanlines() const { return saved_scanlines_.has_value(); }

private:
    struct SavedScanlines {
        std::uint16_t count = 0;
        std::vector<std::uint8_t> pixels;
    };

    DeuterosAmigaFrame frame_;
    std::optional<SavedScanlines> saved_scanlines_;
};

// Compose the channel states in original order. Selector bit 15 chooses the
// masked blit path at $20fb2; unflagged records overwrite all four planes.
// This compatibility helper deliberately begins from a cleared display and
// cannot carry the original save buffer between calls; live sessions use
// DeuterosAmigaCompositor instead.
[[nodiscard]] DeuterosAmigaFrame compose_deuteros_amiga_frame(
    const AmigaAdf& disk, const DeuterosAmigaBundle& bundle,
    const DeuterosAmigaIndexedBlob& blob,
    const std::vector<DeuterosAmigaChannelState>& channels);

[[nodiscard]] std::vector<std::uint8_t> colorize_deuteros_amiga_frame(
    const DeuterosAmigaFrame& frame, const std::array<RgbColor, 16>& palette);

} // namespace eon
