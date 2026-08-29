#include "data/millennium_dos_video_driver.hpp"
#include "data/sha256.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace eon {
namespace {

bool has_bytes(const std::span<const std::uint8_t> bytes, const std::size_t offset,
               const std::span<const std::uint8_t> expected) {
    return offset <= bytes.size() && expected.size() <= bytes.size() - offset
        && std::equal(expected.begin(), expected.end(), bytes.begin() + offset);
}

std::uint16_t little16(const std::span<const std::uint8_t> bytes, const std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw std::runtime_error("Truncated Millennium DOS video-driver dispatch table");
    }
    return static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(bytes[offset + 1] << 8U);
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
    constexpr auto entry = std::to_array<std::uint8_t>({
        0xfb, 0x0e, 0x1f, 0xd1, 0xe0, 0x3d, 0x4e, 0x00, 0x90, 0x73});
    if (!has_bytes(bytes, 0, entry)) throw std::runtime_error("Unsupported Millennium DOS video-driver entry");
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
    const auto zero_prefix = std::array<std::uint8_t, 35>{
        0x26, 0x8a, 0x0f, 0x32, 0xed, 0x80, 0x3e,
        static_cast<std::uint8_t>(ega ? 0x8c : 0xae), 0x00, 0xff, 0x75, 0x07,
        0xb4, 0x0f, 0xcd, 0x10, 0xa2,
        static_cast<std::uint8_t>(ega ? 0x8c : 0xae), 0x00, 0xb8, mode, 0x00,
        0xcd, 0x10, 0xb4, 0x0f, 0xcd, 0x10, 0x3c, mode, 0x74, 0x03,
        0x33, 0xc0, 0xc3};
    constexpr auto ega_four_prefix = std::to_array<std::uint8_t>({
        0x26, 0x8a, 0x07, 0x25, 0x03, 0x00, 0xb1, 0x03,
        0xd2, 0xe0, 0x86, 0x06, 0x8d, 0x00, 0xd2, 0xe8, 0xc3});
    constexpr auto mcga_four_prefix = std::to_array<std::uint8_t>({
        0x26, 0x8a, 0x07, 0x25, 0x03, 0x00, 0x86, 0x06,
        0xaf, 0x00, 0xc3});
    const auto thirty_one_prefix = std::array<std::uint8_t, 6>{
        0xa0, static_cast<std::uint8_t>(ega ? 0x8a : 0xac), 0x00,
        0xb4, static_cast<std::uint8_t>(ega ? 0x04 : 0x01), 0xc3};
    constexpr auto thirteen_prefix = std::to_array<std::uint8_t>({
        0xba, 0xda, 0x03, 0xec, 0xa8, 0x08, 0x75, 0xfb,
        0xec, 0xa8, 0x08, 0x74, 0xfb, 0xc3});
    constexpr auto ega_six_prefix = std::to_array<std::uint8_t>({
        0x26, 0x83, 0x7f, 0x10, 0x00, 0x7e, 0xf8, 0xb8, 0x40,
        0x01, 0x26, 0x2b, 0x47, 0x08, 0x26, 0x3b, 0x47, 0x10});
    constexpr auto mcga_six_prefix = std::to_array<std::uint8_t>({
        0xb8, 0x40, 0x01, 0x26, 0x2b, 0x47, 0x08, 0x26, 0x3b,
        0x47, 0x10, 0x73, 0x04, 0x26, 0x89, 0x47, 0x10});
    constexpr auto ega_six_source_pointer_load = std::to_array<std::uint8_t>({0x26, 0xc5, 0x3f});
    constexpr auto mcga_six_source_pointer_load = std::to_array<std::uint8_t>({0x26, 0xc5, 0x37});
    constexpr auto ega_six_source_prefix = std::to_array<std::uint8_t>({
        0x26, 0xc5, 0x3f, 0x03, 0x6d, 0x04, 0x8b, 0x45, 0x02,
        0x05, 0x03, 0x00, 0xd1, 0xe8, 0xd1, 0xe8, 0x8b, 0xf0,
        0xf7, 0x25});
    constexpr auto mcga_six_source_prefix = std::to_array<std::uint8_t>({
        0x1e, 0x26, 0xc5, 0x37, 0x8b, 0x44, 0x02, 0x8b, 0xf8,
        0xc5, 0x74, 0x04});
    constexpr auto ega_six_source_pointer_load_offset = 0x33U;
    constexpr auto mcga_six_source_pointer_load_offset = 0x29U;
    if (!has_bytes(bytes, function_zero, zero_prefix)
        || (ega && !has_bytes(bytes, function_four, ega_four_prefix))
        || (!ega && !has_bytes(bytes, function_four, mcga_four_prefix))
        || (ega && !has_bytes(bytes, function_six, ega_six_prefix))
        || (!ega && !has_bytes(bytes, function_six, mcga_six_prefix))
        || (ega && !has_bytes(bytes, function_six + ega_six_source_pointer_load_offset,
                              ega_six_source_pointer_load))
        || (!ega && !has_bytes(bytes, function_six + mcga_six_source_pointer_load_offset,
                               mcga_six_source_pointer_load))
        || (ega && !has_bytes(bytes, function_six + ega_six_source_pointer_load_offset,
                              ega_six_source_prefix))
        || (!ega && !has_bytes(bytes, function_six + mcga_six_source_pointer_load_offset - 1,
                               mcga_six_source_prefix))
        || !has_bytes(bytes, function_thirteen, thirteen_prefix)
        || !has_bytes(bytes, function_thirty_one, thirty_one_prefix)) {
        throw std::runtime_error("Unsupported Millennium DOS video-driver ABI profile");
    }
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
