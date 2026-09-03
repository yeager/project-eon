#include "data/millennium_dos_gameplay_screen.hpp"

#include "data/millennium_dos_lib.hpp"
#include "data/sha256.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>

namespace eon {
namespace {

constexpr std::size_t bitmap_header_size = 0x1c;
constexpr std::size_t dac_bytes = 256U * 3U;

[[nodiscard]] std::vector<std::uint8_t> tail_after_pixels(
    std::span<const std::uint8_t> resource, const MillenniumDosBitmap& bitmap) {
    const auto offset = bitmap_header_size + static_cast<std::size_t>(bitmap.encoded_span);
    if (offset > resource.size()) throw std::runtime_error("Invalid Millennium DOS GX bitmap range");
    return {resource.begin() + static_cast<std::ptrdiff_t>(offset), resource.end()};
}

[[nodiscard]] const MillenniumDosLibEntry& require_entry(
    const MillenniumDosLib& library, const char* name) {
    const auto* entry = library.find(name);
    if (!entry) throw std::runtime_error("Missing Millennium DOS GX resource");
    return *entry;
}

void require_profile(const MillenniumDosBitmap& bitmap, const std::uint8_t flags,
                     const std::uint16_t width, const std::uint16_t height,
                     const std::uint8_t max_index, const char* description) {
    if (bitmap.flags != flags || bitmap.codec != 2 || bitmap.width != width
        || bitmap.height != height || bitmap.max_palette_index != max_index) {
        throw std::runtime_error(std::string("Unsupported Millennium DOS GX ") + description);
    }
}

} // namespace

MillenniumDosGameplayScreen parse_millennium_dos_gameplay_screen(
    const std::span<const std::uint8_t> gx_lib) {
    constexpr std::string_view english_gx_library_sha256 =
        "4adf9991226deab4749ac07ad637851994f57d11f6dc45f3f5ce862b5bc34c2f";
    // IMG00/IMG01 are a recovered English DOS presentation boundary, not a
    // generic named-resource convention. Validate the entire original bank
    // before decoding pixels or a DAC table from it.
    if (to_hex(sha256(gx_lib)) != english_gx_library_sha256) {
        throw std::runtime_error("Unsupported Millennium English DOS GX library");
    }
    const MillenniumDosLib library(gx_lib);
    const auto palette_bytes = library.read(require_entry(library, "IMG00"));
    const auto canvas_bytes = library.read(require_entry(library, "IMG01"));

    MillenniumDosGameplayScreen result;
    result.palette_resource = decode_millennium_dos_bitmap(palette_bytes);
    result.canvas = decode_millennium_dos_bitmap(canvas_bytes);
    require_profile(result.palette_resource, 0x0f, 240, 33, 47, "palette resource");
    require_profile(result.canvas, 0x0e, 320, 167, 67, "canvas resource");

    const auto palette_tail = tail_after_pixels(palette_bytes, result.palette_resource);
    const auto palette_count = static_cast<std::size_t>(result.palette_resource.max_palette_index) + 1U;
    if (palette_tail.size() != dac_bytes + palette_count * 3U) {
        throw std::runtime_error("Invalid Millennium DOS GX master VGA palette layout");
    }
    for (std::size_t index = 0; index < 256; ++index) {
        for (std::size_t component = 0; component < 3; ++component) {
            const auto value = palette_tail[index * 3U + component];
            if (value > 0x3fU) {
                throw std::runtime_error("Invalid Millennium DOS GX RGB6 DAC component");
            }
            result.dac_rgb6[index][component] = value;
        }
    }
    result.palette_resource_auxiliary.assign(palette_tail.begin() + static_cast<std::ptrdiff_t>(dac_bytes),
        palette_tail.end());

    const auto canvas_tail = tail_after_pixels(canvas_bytes, result.canvas);
    const auto canvas_count = static_cast<std::size_t>(result.canvas.max_palette_index) + 1U;
    if (canvas_tail.size() != canvas_count * 3U) {
        throw std::runtime_error("Invalid Millennium DOS GX canvas palette layout");
    }
    result.canvas_logical_to_dac.assign(canvas_tail.begin(),
        canvas_tail.begin() + static_cast<std::ptrdiff_t>(canvas_count));
    result.canvas_auxiliary.assign(canvas_tail.begin() + static_cast<std::ptrdiff_t>(canvas_count),
        canvas_tail.end());

    result.rgba.reserve(result.canvas.pixels.size() * 4U);
    for (const auto logical_index : result.canvas.pixels) {
        if (logical_index >= result.canvas_logical_to_dac.size()) {
            throw std::runtime_error("Millennium DOS GX pixel exceeds palette translation");
        }
        const auto& rgb6 = result.dac_rgb6[result.canvas_logical_to_dac[logical_index]];
        for (const auto component : rgb6) {
            result.rgba.push_back(static_cast<std::uint8_t>((component << 2U) | (component >> 4U)));
        }
        result.rgba.push_back(255);
    }
    return result;
}

} // namespace eon
