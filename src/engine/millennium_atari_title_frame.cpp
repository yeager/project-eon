#include "engine/millennium_atari_title_frame.hpp"

#include "data/sha256.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>

namespace eon {
namespace {

constexpr std::size_t title_width = 320;
constexpr std::size_t title_height = 200;
constexpr std::size_t plane_count = 4;
constexpr std::size_t groups_per_row = title_width / 16;
constexpr std::size_t words_per_plane = title_height * groups_per_row;
constexpr std::size_t planar_bytes = title_width * title_height / 2;
constexpr std::size_t packed_file_offset = 6;
constexpr std::size_t palette_source_offset = 0x16c;
constexpr std::size_t initial_palette_offset = 0xf28;
constexpr std::size_t palette_source_bytes = 96;
constexpr std::size_t palette_word_count = 16;
constexpr std::size_t palette_pass_count = 7;

std::uint16_t read_be16(const std::span<const std::uint8_t> bytes,
    const std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw std::runtime_error("Truncated Millennium Atari title palette word");
    }
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(bytes[offset]) << 8U) | bytes[offset + 1]);
}

void require_range(const std::span<const std::uint8_t> bytes,
    const std::size_t offset, const std::size_t count, const char* message) {
    if (offset > bytes.size() || count > bytes.size() - offset) {
        throw std::runtime_error(message);
    }
}

std::uint8_t expand_component(const std::uint16_t word, const unsigned shift) {
    const auto component = static_cast<std::uint16_t>((word >> shift) & 7U);
    return static_cast<std::uint8_t>((component * 255U + 3U) / 7U);
}

} // namespace

MillenniumAtariTitleFrameSnapshot decode_millennium_atari_title_frame(
    const std::span<const std::uint8_t> mill22a,
    const std::span<const std::uint8_t> mill22c,
    const std::uint64_t generation) {
    constexpr std::string_view expected_a_sha256 =
        "74d7d630779fd811aedcdbe31b14e54198eb9ffd673df512dd70b6165c4a37b6";
    constexpr std::string_view expected_c_sha256 =
        "a28a49eea33a14210193bbe6e36abf95700ac6789681bf1a9eac5d09a0999055";
    if (generation == 0 || mill22a.size() != 7506U || mill22c.size() != 9597U
        || to_hex(sha256(mill22a)) != expected_a_sha256
        || to_hex(sha256(mill22c)) != expected_c_sha256) {
        throw std::runtime_error("Unsupported Millennium Atari title source pair");
    }
    require_range(mill22a, palette_source_offset, palette_source_bytes,
        "Millennium Atari title palette source is incomplete");
    require_range(mill22a, initial_palette_offset, palette_word_count * 2U,
        "Millennium Atari initial palette is incomplete");

    MillenniumAtariTitleFrameSnapshot result;
    result.generation = generation;
    result.planar_bytes.assign(planar_bytes, 0);
    std::size_t input = packed_file_offset;
    std::size_t written_words = 0;
    const auto write_pair = [&](const std::uint8_t first, const std::uint8_t second) {
        if (written_words >= plane_count * words_per_plane) return;
        const auto plane = written_words / words_per_plane;
        const auto in_plane = written_words % words_per_plane;
        const auto row = in_plane / groups_per_row;
        const auto group = in_plane % groups_per_row;
        const auto address = row * (title_width / 2U) + group * 8U + plane * 2U;
        result.planar_bytes[address] = first;
        result.planar_bytes[address + 1U] = second;
        ++written_words;
    };
    const auto take_byte = [&]() -> std::uint8_t {
        if (input >= mill22c.size()) {
            throw std::runtime_error("Truncated Millennium Atari title token");
        }
        return mill22c[input++];
    };
    const auto repeat_pair = [&](const std::size_t count,
        const std::uint8_t first, const std::uint8_t second) {
        const auto remaining = plane_count * words_per_plane - written_words;
        const auto writes = std::min(count, remaining);
        for (std::size_t index = 0; index < writes; ++index) write_pair(first, second);
    };

    while (input < mill22c.size() && written_words < plane_count * words_per_plane) {
        const auto token = take_byte();
        switch (token >> 6U) {
        case 0: {
            // The zero counter wraps as a byte, so zero denotes 256 literal
            // words. Each source pair is consumed even at a plane boundary.
            const auto count = token == 0 ? 256U : static_cast<std::size_t>(token);
            for (std::size_t index = 0; index < count; ++index) {
                const auto first = take_byte();
                const auto second = take_byte();
                write_pair(first, second);
            }
            break;
        }
        case 1: {
            const auto count = static_cast<std::size_t>(token & 0x3fU);
            const auto value = take_byte();
            repeat_pair(count == 0 ? 65536U : count, value, value);
            break;
        }
        case 2: {
            const auto low_count = take_byte();
            const auto encoded_count = (static_cast<std::size_t>(token & 0x3fU) << 8U)
                | low_count;
            const auto first = take_byte();
            const auto second = take_byte();
            repeat_pair(encoded_count == 0 ? 65536U : encoded_count, first, second);
            break;
        }
        case 3: {
            const auto count = static_cast<std::size_t>(token & 0x3fU);
            const auto first = take_byte();
            const auto second = take_byte();
            repeat_pair(count == 0 ? 65536U : count, second, first);
            break;
        }
        }
    }
    if (input != mill22c.size() || written_words != plane_count * words_per_plane) {
        throw std::runtime_error("Millennium Atari title token stream does not match frame bounds");
    }

    std::array<std::uint8_t, palette_source_bytes> palette_source{};
    std::copy_n(mill22a.begin() + static_cast<std::ptrdiff_t>(palette_source_offset),
        palette_source.size(), palette_source.begin());
    for (std::size_t index = 0; index < result.palette_rgb3.size(); ++index) {
        result.palette_rgb3[index] = read_be16(mill22a,
            initial_palette_offset + index * 2U);
        if ((result.palette_rgb3[index] & 0xf888U) != 0) {
            throw std::runtime_error("Millennium Atari initial palette word is not RGB3");
        }
    }
    for (std::size_t pass = 0; pass < palette_pass_count; ++pass) {
        for (std::size_t group = 0; group < palette_word_count; ++group) {
            std::uint16_t carry_weight = 0x0100;
            for (std::size_t lane = 0; lane < 3; ++lane) {
                const auto source_index = group * 6U + lane * 2U;
                const auto sum = static_cast<std::uint16_t>(palette_source[source_index]
                    + palette_source[source_index + 1U]);
                if (sum > 0xffU) {
                    result.palette_rgb3[group] = static_cast<std::uint16_t>(
                        result.palette_rgb3[group] + carry_weight);
                }
                palette_source[source_index] = static_cast<std::uint8_t>(sum);
                carry_weight = static_cast<std::uint16_t>(carry_weight >> 4U);
            }
        }
    }

    result.color_indices.reserve(title_width * title_height);
    result.rgba.reserve(title_width * title_height * 4U);
    for (std::size_t row = 0; row < title_height; ++row) {
        for (std::size_t group = 0; group < groups_per_row; ++group) {
            const auto address = row * (title_width / 2U) + group * 8U;
            std::array<std::uint16_t, plane_count> words{};
            for (std::size_t plane = 0; plane < plane_count; ++plane) {
                words[plane] = read_be16(result.planar_bytes, address + plane * 2U);
            }
            for (std::size_t bit = 0; bit < 16; ++bit) {
                std::uint8_t color = 0;
                for (std::size_t plane = 0; plane < plane_count; ++plane) {
                    color = static_cast<std::uint8_t>(color
                        | (((words[plane] >> (15U - bit)) & 1U) << plane));
                }
                result.color_indices.push_back(color);
                const auto palette_word = result.palette_rgb3[color];
                result.rgba.insert(result.rgba.end(), {
                    expand_component(palette_word, 8U),
                    expand_component(palette_word, 4U),
                    expand_component(palette_word, 0U), 0xff});
            }
        }
    }

    result.planar_sha256 = to_hex(sha256(result.planar_bytes));
    result.color_indices_sha256 = to_hex(sha256(result.color_indices));
    std::array<std::uint8_t, palette_word_count * 2U> palette_bytes{};
    for (std::size_t index = 0; index < result.palette_rgb3.size(); ++index) {
        palette_bytes[index * 2U] = static_cast<std::uint8_t>(result.palette_rgb3[index] >> 8U);
        palette_bytes[index * 2U + 1U] = static_cast<std::uint8_t>(result.palette_rgb3[index]);
    }
    result.palette_sha256 = to_hex(sha256(palette_bytes));
    result.rgba_sha256 = to_hex(sha256(result.rgba));
    if (result.planar_sha256 != "97ca82e4296c649a853d45f3900f90b2b8490c1cf12c0580258b3c89e13b4475"
        || result.color_indices_sha256 != "1bc61a2ecf3dd75a317996e56bf836f4e0aa304c496d5ec2d366da53fd64d8b8"
        || result.palette_sha256 != "19e71f573ee6cec84d629becc821360b8384766ca6f9a308f7c2b0d67922cb02"
        || result.rgba_sha256 != "d4d37cfc50ea59fe3453bb2bf4c3e8b3880cd31261bee5b7f02a9a68c3190595") {
        throw std::runtime_error("Millennium Atari title frame disagrees with source-derived anchors");
    }
    return result;
}

} // namespace eon
