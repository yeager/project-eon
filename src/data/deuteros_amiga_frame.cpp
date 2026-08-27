#include "data/deuteros_amiga_frame.hpp"

#include <algorithm>
#include <stdexcept>

namespace eon {
namespace {

constexpr std::size_t frame_pixels = static_cast<std::size_t>(DeuterosAmigaFrame::width)
    * DeuterosAmigaFrame::height;

void draw_bitmap(DeuterosAmigaFrame& frame, const DeuterosAmigaBitmap& bitmap,
    const DeuterosAmigaChannelState& channel, bool transparent) {
    const auto origin_x = static_cast<int>(channel.x) * 16;
    const auto origin_y = static_cast<int>(channel.y);
    if (origin_x < 0 || origin_x + bitmap.width > DeuterosAmigaFrame::width) {
        // $20d8e/$20fb2 trust the word coordinate. Refuse to create a
        // different clipped image when malformed input would overrun a plane.
        throw std::runtime_error("Deuteros bitmap outside horizontal screen bounds");
    }
    for (int source_y = 0; source_y < bitmap.height; ++source_y) {
        const auto destination_y = origin_y + source_y;
        if (destination_y < 0 || destination_y >= DeuterosAmigaFrame::height) continue;
        for (int source_x = 0; source_x < bitmap.width; ++source_x) {
            const auto color = bitmap.color_indices[
                static_cast<std::size_t>(source_y) * bitmap.width + source_x];
            if (transparent && color == 0) continue;
            frame.color_indices[static_cast<std::size_t>(destination_y)
                * DeuterosAmigaFrame::width + static_cast<std::size_t>(origin_x + source_x)] = color;
        }
    }
}

std::uint16_t visible_scanline_count(const DeuterosAmigaBitmap& bitmap,
    const DeuterosAmigaChannelState& channel) {
    // $20dca..$20dd6 clips the lower edge before $21034 saves the result.
    // The recovered programs do not feed a negative save origin to this
    // unsigned 68000 path; reject it rather than assigning invented meaning.
    if (channel.y < 0) throw std::runtime_error("Deuteros scanline save has negative origin");
    const auto remaining = static_cast<int>(DeuterosAmigaFrame::height) - channel.y;
    if (remaining <= 0) return 0;
    return static_cast<std::uint16_t>(std::min<int>(bitmap.height, remaining));
}

} // namespace

DeuterosAmigaCompositor::DeuterosAmigaCompositor() { reset(); }

void DeuterosAmigaCompositor::reset() {
    frame_.color_indices.assign(frame_pixels, 0);
    saved_scanlines_.reset();
}

const DeuterosAmigaFrame& DeuterosAmigaCompositor::compose(
    const AmigaAdf& disk, const DeuterosAmigaBundle& bundle,
    const DeuterosAmigaIndexedBlob& blob,
    std::vector<DeuterosAmigaChannelState>& channels) {
    if (frame_.color_indices.size() != frame_pixels) {
        throw std::runtime_error("Invalid persistent Deuteros display dimensions");
    }
    for (auto& channel : channels) {
        if (!channel.active || channel.bitmap_selector == 0xff) continue;
        if (channel.bitmap_selector == 0xfe) {
            // $21468 loads A4 from state +12 and calls $20580. Its bounded
            // route writes global original video pointers through
            // DeuterosAmigaOpening after this channel pass; do not invent a
            // bitmap interpretation for untraced streams here.
            continue;
        }
        const auto selector = channel.bitmap_selector;
        if ((selector & 0x2000U) != 0) {
            // $20c9a branches directly to $21092. There is exactly one global
            // $23024 buffer, so do not fabricate selector-specific storage.
            if (!saved_scanlines_) {
                throw std::runtime_error("Deuteros scanline restore without saved buffer");
            }
            if (channel.y < 0 || channel.y >= DeuterosAmigaFrame::height) {
                throw std::runtime_error("Deuteros scanline restore outside display");
            }
            if (saved_scanlines_->pixels.size()
                != static_cast<std::size_t>(saved_scanlines_->count) * DeuterosAmigaFrame::width) {
                throw std::runtime_error("Invalid Deuteros saved scanline buffer");
            }
            const auto count = std::min<std::size_t>(saved_scanlines_->count,
                static_cast<std::size_t>(DeuterosAmigaFrame::height - channel.y));
            std::copy_n(saved_scanlines_->pixels.begin(), count * DeuterosAmigaFrame::width,
                frame_.color_indices.begin() + static_cast<std::size_t>(channel.y)
                    * DeuterosAmigaFrame::width);
            continue;
        }

        const auto bitmap = decode_deuteros_amiga_bitmap(
            disk, bundle, blob, static_cast<std::uint8_t>(selector));
        const bool masked = (selector & 0x8000U) != 0;
        const bool save_scanlines = (selector & 0xc000U) == 0xc000U;
        // $20cb0 strips both flags and calls $20d8e, so this is opaque rather
        // than the $20fb2 transparent path used by bit 15 alone.
        draw_bitmap(frame_, bitmap, channel, masked && !save_scanlines);
        if (save_scanlines) {
            const auto count = visible_scanline_count(bitmap, channel);
            SavedScanlines saved;
            saved.count = count;
            saved.pixels.resize(static_cast<std::size_t>(count) * DeuterosAmigaFrame::width);
            std::copy_n(frame_.color_indices.begin() + static_cast<std::size_t>(channel.y)
                    * DeuterosAmigaFrame::width,
                saved.pixels.size(), saved.pixels.begin());
            saved_scanlines_ = std::move(saved);
            // $21056 changes the selector in the real 24-byte channel state.
            channel.bitmap_selector = 0xffffU;
        }
    }
    return frame_;
}

DeuterosAmigaFrame compose_deuteros_amiga_frame(
    const AmigaAdf& disk, const DeuterosAmigaBundle& bundle,
    const DeuterosAmigaIndexedBlob& blob,
    const std::vector<DeuterosAmigaChannelState>& channels) {
    auto copy = channels;
    DeuterosAmigaCompositor compositor;
    return compositor.compose(disk, bundle, blob, copy);
}

std::vector<std::uint8_t> colorize_deuteros_amiga_frame(
    const DeuterosAmigaFrame& frame, const std::array<RgbColor, 16>& palette) {
    if (frame.color_indices.size() != frame_pixels) {
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
