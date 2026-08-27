#include "data/millennium_dos_video_driver.hpp"

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

MillenniumDosVideoDriverProfile parse_millennium_dos_video_driver(
    const std::span<const std::uint8_t> bytes, const MillenniumDosVideoDriverKind kind) {
    const bool ega = kind == MillenniumDosVideoDriverKind::ega640;
    const auto dispatch = static_cast<std::uint16_t>(ega ? 0x20 : 0x32);
    const auto expected_size = static_cast<std::size_t>(ega ? 4632 : 4366);
    if (bytes.size() != expected_size) throw std::runtime_error("Unsupported Millennium DOS video-driver size");
    constexpr auto entry = std::to_array<std::uint8_t>({
        0xfb, 0x0e, 0x1f, 0xd1, 0xe0, 0x3d, 0x4e, 0x00, 0x90, 0x73});
    if (!has_bytes(bytes, 0, entry)) throw std::runtime_error("Unsupported Millennium DOS video-driver entry");
    const auto function_zero = little16(bytes, dispatch);
    const auto function_four = little16(bytes, dispatch + 8);
    const auto expected_zero = static_cast<std::uint16_t>(ega ? 0x1c8 : 0x1e6);
    const auto expected_four = static_cast<std::uint16_t>(ega ? 0xc17 : 0x815);
    const auto mode = static_cast<std::uint8_t>(ega ? 0x0e : 0x13);
    const auto state = static_cast<std::uint16_t>(ega ? 0x8d : 0xaf);
    if (function_zero != expected_zero || function_four != expected_four) {
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
    if (!has_bytes(bytes, function_zero, zero_prefix)
        || (ega && !has_bytes(bytes, function_four, ega_four_prefix))
        || (!ega && !has_bytes(bytes, function_four, mcga_four_prefix))) {
        throw std::runtime_error("Unsupported Millennium DOS video-driver ABI profile");
    }
    return {
        .kind = kind,
        .byte_size = bytes.size(),
        .dispatch_table_address = dispatch,
        .function_zero_address = function_zero,
        .function_four_address = function_four,
        .function_zero_video_mode = mode,
        .function_zero_set_mode_interrupt_site = static_cast<std::uint16_t>(function_zero + 0x16),
        .function_zero_mode_mismatch_return = static_cast<std::uint16_t>(function_zero + 0x22),
        .function_four_input_offset = 0,
        .function_four_input_mask = 3,
        .function_four_state_address = state,
    };
}

} // namespace eon
