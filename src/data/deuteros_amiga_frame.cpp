#include "data/deuteros_amiga_frame.hpp"

#include <stdexcept>

namespace eon {

DeuterosAmigaFrame compose_deuteros_amiga_frame(
    const AmigaAdf& disk, const DeuterosAmigaBundle& bundle,
    const DeuterosAmigaIndexedBlob& blob,
    const std::vector<DeuterosAmigaChannelState>& channels) {
    DeuterosAmigaFrame frame;
    frame.color_indices.assign(
        static_cast<std::size_t>(DeuterosAmigaFrame::width) * DeuterosAmigaFrame::height, 0);
    for (const auto& channel : channels) {
        if (!channel.active || channel.bitmap_selector == 0xff) continue;
        if (channel.bitmap_selector == 0xfe) {
            // $20580 consumes the alternate resource selected by opcode $0f;
            // it is a separate verified format and must not be treated as RLE.
            continue;
        }
        if ((channel.bitmap_selector & 0x2000U) != 0
            || (channel.bitmap_selector & 0xc000U) == 0xc000U) {
            throw std::runtime_error("Deuteros frame requires stateful scanline save/restore");
        }
        const auto record = static_cast<std::uint8_t>(channel.bitmap_selector);
        const auto bitmap = decode_deuteros_amiga_bitmap(disk, bundle, blob, record);
        const bool transparent = (channel.bitmap_selector & 0x8000U) != 0;
        const auto origin_x = static_cast<int>(channel.x) * 16;
        const auto origin_y = static_cast<int>(channel.y);
        if (origin_x < 0 || origin_x + bitmap.width > DeuterosAmigaFrame::width) {
            // Original code trusts x and performs no horizontal clipping.
            // Native import fails closed instead of writing outside a plane.
            throw std::runtime_error("Deuteros bitmap outside horizontal screen bounds");
        }
        for (int source_y = 0; source_y < bitmap.height; ++source_y) {
            const auto destination_y = origin_y + source_y;
            if (destination_y < 0 || destination_y >= DeuterosAmigaFrame::height) continue;
            for (int source_x = 0; source_x < bitmap.width; ++source_x) {
                const auto destination_x = origin_x + source_x;
                if (destination_x < 0 || destination_x >= DeuterosAmigaFrame::width) continue;
                const auto color = bitmap.color_indices[
                    static_cast<std::size_t>(source_y) * bitmap.width + source_x];
                if (transparent && color == 0) continue;
                frame.color_indices[static_cast<std::size_t>(destination_y)
                    * DeuterosAmigaFrame::width + destination_x] = color;
            }
        }
    }
    return frame;
}

std::vector<std::uint8_t> colorize_deuteros_amiga_frame(
    const DeuterosAmigaFrame& frame, const std::array<RgbColor, 16>& palette) {
    if (frame.color_indices.size()
        != static_cast<std::size_t>(DeuterosAmigaFrame::width) * DeuterosAmigaFrame::height) {
        throw std::runtime_error("Invalid Deuteros frame dimensions");
    }
    std::vector<std::uint8_t> rgba;
    rgba.reserve(frame.color_indices.size() * 4);
    for (const auto index : frame.color_indices) {
        if (index >= palette.size()) throw std::runtime_error("Deuteros frame palette index outside range");
        const auto color = palette[index];
        rgba.insert(rgba.end(), {color.red, color.green, color.blue, 0xff});
    }
    return rgba;
}

} // namespace eon
