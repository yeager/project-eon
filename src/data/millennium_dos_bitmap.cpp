#include "data/millennium_dos_bitmap.hpp"

#include <array>
#include <cstddef>
#include <stdexcept>

namespace eon {
namespace {

constexpr std::size_t header_size = 0x1c;
constexpr std::size_t dac_entry_count = 256;
constexpr std::size_t dac_bytes = dac_entry_count * 3;

std::uint16_t little16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw std::runtime_error("Truncated Millennium DOS bitmap field");
    }
    return static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
}

class NibbleReader {
public:
    explicit NibbleReader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    std::uint8_t read() {
        if (byte_offset_ >= bytes_.size()) {
            throw std::runtime_error("Truncated Millennium DOS bitmap command stream");
        }
        const auto value = high_nibble_
            ? static_cast<std::uint8_t>(bytes_[byte_offset_] >> 4U)
            : static_cast<std::uint8_t>(bytes_[byte_offset_] & 0x0fU);
        if (high_nibble_) ++byte_offset_;
        high_nibble_ = !high_nibble_;
        return value;
    }

    std::uint8_t read_byte() {
        const auto low = read();
        return static_cast<std::uint8_t>(low | static_cast<std::uint8_t>(read() << 4U));
    }

private:
    std::span<const std::uint8_t> bytes_;
    std::size_t byte_offset_ = 0;
    bool high_nibble_ = false;
};

} // namespace

MillenniumDosBitmap decode_millennium_dos_bitmap(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < header_size) {
        throw std::runtime_error("Millennium DOS bitmap is too short");
    }

    MillenniumDosBitmap result;
    result.flags = bytes[0];
    result.max_palette_index = bytes[1];
    result.codec = bytes[4];
    result.deduction = little16(bytes, 0x14);
    result.height = little16(bytes, 0x16);
    result.width = little16(bytes, 0x18);
    result.encoded_span = little16(bytes, 0x1a);

    if (result.codec != 2) {
        throw std::runtime_error("Unsupported Millennium DOS bitmap codec");
    }
    if (result.width == 0 || result.height == 0) {
        throw std::runtime_error("Invalid Millennium DOS bitmap dimensions");
    }
    if (result.encoded_span == 0
        || result.encoded_span > bytes.size() - header_size) {
        throw std::runtime_error("Invalid Millennium DOS bitmap stream range");
    }

    const auto pixel_count = static_cast<std::size_t>(result.width)
        * static_cast<std::size_t>(result.height);
    if (pixel_count > result.pixels.max_size()) {
        throw std::runtime_error("Millennium DOS bitmap dimensions overflow");
    }

    std::array<std::uint8_t, 14> deltas{};
    for (std::size_t index = 0; index < deltas.size(); ++index) {
        deltas[index] = bytes[5 + index];
    }

    const auto stream = bytes.subspan(header_size, result.encoded_span);
    result.pixels.reserve(pixel_count);
    if (stream.front() > result.max_palette_index) {
        throw std::runtime_error("Invalid initial Millennium DOS palette index");
    }
    result.pixels.push_back(stream.front());
    NibbleReader reader(stream.subspan(1));
    const auto palette_size = static_cast<std::uint16_t>(result.max_palette_index) + 1U;

    auto append = [&](std::uint8_t value) {
        if (value > result.max_palette_index) {
            throw std::runtime_error("Invalid Millennium DOS palette index");
        }
        if (result.pixels.size() >= pixel_count) {
            throw std::runtime_error("Millennium DOS bitmap output overrun");
        }
        result.pixels.push_back(value);
    };

    while (result.pixels.size() < pixel_count) {
        const auto control = reader.read();
        if (control <= 0x0d) {
            const auto next = static_cast<std::uint16_t>(result.pixels.back())
                + static_cast<std::uint16_t>(deltas[control]);
            append(static_cast<std::uint8_t>(next % palette_size));
            continue;
        }
        if (control == 0x0f) {
            append(reader.read_byte());
            continue;
        }

        auto repeats = static_cast<std::uint32_t>(reader.read_byte());
        if (repeats == 0xffU) {
            const auto high = static_cast<std::uint32_t>(reader.read_byte());
            const auto low = static_cast<std::uint32_t>(reader.read_byte());
            repeats = (high << 8U) | low;
        }
        repeats += 2U;
        if (repeats > pixel_count - result.pixels.size()) {
            throw std::runtime_error("Millennium DOS bitmap run overruns output");
        }
        result.pixels.insert(result.pixels.end(), repeats, result.pixels.back());
    }

    return result;
}

MillenniumDosPalette decode_millennium_dos_palette(
    std::span<const std::uint8_t> bytes, const MillenniumDosBitmap& bitmap) {
    if ((bitmap.flags & 0x01U) == 0U) {
        throw std::runtime_error("Millennium DOS bitmap has no VGA palette block");
    }
    const auto palette_offset = header_size + static_cast<std::size_t>(bitmap.encoded_span);
    const auto translation_count = static_cast<std::size_t>(bitmap.max_palette_index) + 1U;
    if (palette_offset > bytes.size() || bytes.size() - palette_offset < dac_bytes
        || bytes.size() - palette_offset - dac_bytes != translation_count * 2U) {
        throw std::runtime_error("Invalid Millennium DOS VGA palette layout");
    }

    MillenniumDosPalette result;
    for (std::size_t index = 0; index < dac_entry_count; ++index) {
        for (std::size_t component = 0; component < 3; ++component) {
            const auto value = bytes[palette_offset + index * 3U + component];
            if (value > 0x3fU) {
                throw std::runtime_error("Invalid Millennium DOS RGB6 DAC component");
            }
            result.dac_rgb6[index][component] = value;
        }
    }
    const auto auxiliary_offset = palette_offset + dac_bytes;
    const auto translation_offset = auxiliary_offset + translation_count;
    result.auxiliary_translation.assign(bytes.begin() + static_cast<std::ptrdiff_t>(auxiliary_offset),
        bytes.begin() + static_cast<std::ptrdiff_t>(translation_offset));
    result.logical_to_dac.assign(bytes.begin() + static_cast<std::ptrdiff_t>(translation_offset),
        bytes.end());
    return result;
}

std::vector<std::uint8_t> colorize_millennium_dos_bitmap(
    const MillenniumDosBitmap& bitmap, const MillenniumDosPalette& palette) {
    const auto expected_pixels = static_cast<std::size_t>(bitmap.width)
        * static_cast<std::size_t>(bitmap.height);
    if (bitmap.pixels.size() != expected_pixels
        || palette.logical_to_dac.size() != static_cast<std::size_t>(bitmap.max_palette_index) + 1U) {
        throw std::runtime_error("Incompatible Millennium DOS bitmap palette");
    }
    std::vector<std::uint8_t> result;
    result.reserve(expected_pixels * 4U);
    for (const auto logical_index : bitmap.pixels) {
        if (logical_index >= palette.logical_to_dac.size()) {
            throw std::runtime_error("Millennium DOS pixel exceeds palette translation");
        }
        const auto& rgb6 = palette.dac_rgb6[palette.logical_to_dac[logical_index]];
        for (const auto component : rgb6) {
            result.push_back(static_cast<std::uint8_t>((component << 2U) | (component >> 4U)));
        }
        result.push_back(255);
    }
    return result;
}

} // namespace eon
