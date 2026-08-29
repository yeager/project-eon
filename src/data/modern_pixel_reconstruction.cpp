#include "data/modern_pixel_reconstruction.hpp"

#include <array>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace eon {
namespace {

constexpr std::size_t channels = 4;
constexpr std::size_t maximum_source_pixels = 16U * 1024U * 1024U;

using Pixel = std::array<std::uint8_t, channels>;

Pixel pixel_at(const std::span<const std::uint8_t> source, const int width,
    const int x, const int y) {
    const auto offset = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
        + static_cast<std::size_t>(x)) * channels;
    Pixel pixel{};
    for (std::size_t channel = 0; channel < channels; ++channel) pixel[channel] = source[offset + channel];
    return pixel;
}

void write_pixel(std::vector<std::uint8_t>& output, const int width, const int x,
    const int y, const Pixel& pixel) {
    const auto offset = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
        + static_cast<std::size_t>(x)) * channels;
    for (std::size_t channel = 0; channel < channels; ++channel) output[offset + channel] = pixel[channel];
}

} // namespace

ModernReconstructedSurface reconstruct_rgba_scale2x(
    const std::span<const std::uint8_t> original_rgba, const int width, const int height) {
    if (width <= 0 || height <= 0) throw std::runtime_error("Scale2x source dimensions must be positive");
    const auto source_width = static_cast<std::size_t>(width);
    const auto source_height = static_cast<std::size_t>(height);
    if (source_width > maximum_source_pixels / source_height) {
        throw std::runtime_error("Scale2x source exceeds bounded pixel budget");
    }
    const auto source_pixels = source_width * source_height;
    if (original_rgba.size() != source_pixels * channels) {
        throw std::runtime_error("Scale2x source byte count does not match RGBA dimensions");
    }
    if (width > std::numeric_limits<int>::max() / 2
        || height > std::numeric_limits<int>::max() / 2) {
        throw std::runtime_error("Scale2x output dimensions overflow");
    }

    ModernReconstructedSurface result;
    result.width = width * 2;
    result.height = height * 2;
    result.rgba.resize(source_pixels * channels * 4U);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto b = pixel_at(original_rgba, width, x, y > 0 ? y - 1 : y);
            const auto d = pixel_at(original_rgba, width, x > 0 ? x - 1 : x, y);
            const auto e = pixel_at(original_rgba, width, x, y);
            const auto f = pixel_at(original_rgba, width, x + 1 < width ? x + 1 : x, y);
            const auto h = pixel_at(original_rgba, width, x, y + 1 < height ? y + 1 : y);

            // The standard Scale2x decision rule. All chosen pixels originate
            // from this verified decoded surface; no colour is invented.
            const auto e0 = d == b && d != h && b != f ? d : e;
            const auto e1 = b == f && b != d && f != h ? f : e;
            const auto e2 = d == h && d != b && h != f ? d : e;
            const auto e3 = h == f && d != h && b != f ? f : e;
            write_pixel(result.rgba, result.width, x * 2, y * 2, e0);
            write_pixel(result.rgba, result.width, x * 2 + 1, y * 2, e1);
            write_pixel(result.rgba, result.width, x * 2, y * 2 + 1, e2);
            write_pixel(result.rgba, result.width, x * 2 + 1, y * 2 + 1, e3);
        }
    }
    return result;
}

} // namespace eon
