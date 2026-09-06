#include "data/millennium_dos_video_driver.hpp"
#include "data/sha256.hpp"

#include <stdexcept>

namespace eon {
namespace {

std::uint16_t little16(const std::span<const std::uint8_t> bytes, const std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw std::runtime_error("Truncated Millennium DOS video-driver dispatch table");
    }
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(bytes[offset])
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U));
}

} // namespace

namespace {

MillenniumDosVideoDriverProfile parse_driver_profile(
    const std::span<const std::uint8_t> bytes, const MillenniumDosVideoDriverKind kind,
    const bool spanish) {
    const bool ega = kind == MillenniumDosVideoDriverKind::ega640;
    const auto dispatch = static_cast<std::uint16_t>(ega ? 0x20 : 0x32);
    const auto english_size = static_cast<std::size_t>(ega ? 4632 : 4366);
    const auto spanish_size = static_cast<std::size_t>(ega ? 4630 : 4346);
    if (bytes.size() != (spanish ? spanish_size : english_size)) {
        throw std::runtime_error("Unsupported Millennium DOS video-driver size");
    }
    // A matching dispatch table is not enough to make a foreign or altered
    // driver executable evidence. English drivers are admitted by their full
    // supplied leaf identity just like the Spanish FAT12 leaves below.
    if (!spanish) {
        const auto expected = ega
            ? "ba003dd155fee868980f6ece933c33f9b22af68ed376cd64f4e027abd65baf6a"
            : "bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228";
        if (to_hex(sha256(bytes)) != expected) {
            throw std::runtime_error("Unsupported Millennium English DOS video driver");
        }
    }
    const auto function_zero = little16(bytes, dispatch);
    const auto function_four = little16(bytes, dispatch + 8);
    const auto function_six = little16(bytes, dispatch + 12);
    const auto function_thirteen = little16(bytes, dispatch + 0x26);
    const auto function_thirty_one = little16(bytes, dispatch + 0x3e);
    const auto expected_zero = static_cast<std::uint16_t>(ega ? 0x1c8 : 0x1e6);
    const auto expected_four = static_cast<std::uint16_t>(ega ? 0xc17 : 0x815);
    const auto expected_six = static_cast<std::uint16_t>(ega ? 0x8a6 : 0x705);
    const auto expected_thirteen = static_cast<std::uint16_t>(ega ? 0xd37 : 0x905);
    const auto expected_thirty_one = static_cast<std::uint16_t>(ega ? 0x235 : 0x24c);
    const auto mode = static_cast<std::uint8_t>(ega ? 0x0e : 0x13);
    const auto state = static_cast<std::uint16_t>(ega ? 0x8d : 0xaf);
    if (function_zero != expected_zero || function_four != expected_four || function_six != expected_six
        || function_thirteen != expected_thirteen
        || function_thirty_one != expected_thirty_one) {
        throw std::runtime_error("Unsupported Millennium DOS video-driver dispatch targets");
    }
    constexpr auto ega_six_source_pointer_load_offset = 0x33U;
    constexpr auto mcga_six_source_pointer_load_offset = 0x29U;
    return {
        .kind = kind,
        .byte_size = bytes.size(),
        .dispatch_table_address = dispatch,
        .function_zero_address = function_zero,
        .function_zero_input_offset = 0,
        .function_zero_cached_mode_address = static_cast<std::uint16_t>(ega ? 0x8c : 0xae),
        .function_zero_cached_mode_unknown_sentinel = 0xff,
        .function_zero_cached_mode_query_interrupt_site = static_cast<std::uint16_t>(function_zero + 0x0f),
        .function_zero_cached_mode_unknown_branch_target = static_cast<std::uint16_t>(function_zero + 0x14),
        .function_four_address = function_four,
        .function_zero_video_mode = mode,
        .function_zero_set_mode_interrupt_site = static_cast<std::uint16_t>(function_zero + 0x16),
        .function_zero_verify_mode_interrupt_site = static_cast<std::uint16_t>(function_zero + 0x1b),
        .function_zero_mode_match_return = static_cast<std::uint16_t>(function_zero + 0x24),
        .function_zero_mode_mismatch_return = static_cast<std::uint16_t>(function_zero + 0x22),
        .function_four_input_offset = 0,
        .function_four_input_mask = 3,
        .function_four_state_address = state,
        .function_six_address = function_six,
        .function_six_source_pointer_offset = 0,
        .function_six_source_pointer_load_address = static_cast<std::uint16_t>(
            function_six + (ega ? ega_six_source_pointer_load_offset : mcga_six_source_pointer_load_offset)),
        .function_six_source_word_zero_read_address = static_cast<std::uint16_t>(
            ega ? function_six + 0x45 : 0),
        .function_six_source_word_two_read_address = static_cast<std::uint16_t>(
            ega ? function_six + 0x39 : function_six + 0x2c),
        .function_six_source_word_four_read_address = static_cast<std::uint16_t>(
            ega ? function_six + 0x36 : 0),
        .function_six_source_nested_pointer_load_address = static_cast<std::uint16_t>(
            ega ? 0 : function_six + 0x31),
        .function_six_screen_width = 0x140,
        .function_six_horizontal_offset = 8,
        .function_six_height_offset = 0x10,
        .function_thirteen_address = function_thirteen,
        .function_thirteen_status_port = 0x03da,
        .function_thirteen_retrace_mask = 0x08,
        .function_thirteen_first_poll_address = static_cast<std::uint16_t>(function_thirteen + 3),
        .function_thirteen_second_poll_address = static_cast<std::uint16_t>(function_thirteen + 8),
        .function_thirty_one_address = function_thirty_one,
        .function_thirty_one_state_address = static_cast<std::uint16_t>(ega ? 0x8a : 0xac),
        .function_thirty_one_return_ah = static_cast<std::uint8_t>(ega ? 0x04 : 0x01),
    };
}

} // namespace

MillenniumDosVideoDriverProfile parse_millennium_dos_video_driver(
    const std::span<const std::uint8_t> bytes, const MillenniumDosVideoDriverKind kind) {
    return parse_driver_profile(bytes, kind, false);
}

MillenniumDosVideoDriverProfile parse_millennium_dos_spanish_video_driver(
    const std::span<const std::uint8_t> bytes, const MillenniumDosVideoDriverKind kind) {
    const auto expected = kind == MillenniumDosVideoDriverKind::ega640
        ? "ef031b0b6e720ab2dafc1eb6373ddb76e0ff15f7b59ac785265c5136be153daf"
        : "3fb76b2ccccffc304b0525cd410b940bbb61e3d1a7a90340d72e5683d7f0211d";
    if (to_hex(sha256(bytes)) != expected) {
        throw std::runtime_error("Unsupported Millennium Spanish DOS video driver");
    }
    return parse_driver_profile(bytes, kind, true);
}

} // namespace eon
