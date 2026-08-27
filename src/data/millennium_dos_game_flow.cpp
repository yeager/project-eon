#include "data/millennium_dos_game_flow.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace eon {
namespace {

bool has_bytes(std::span<const std::uint8_t> bytes, std::size_t offset,
               std::span<const std::uint8_t> expected) {
    return offset <= bytes.size() && expected.size() <= bytes.size() - offset
        && std::equal(expected.begin(), expected.end(), bytes.begin()
            + static_cast<std::ptrdiff_t>(offset));
}

} // namespace

MillenniumDosGameFlow parse_millennium_dos_game_flow(
    const std::span<const std::uint8_t> game_executable) {
    // 2200AD.EXE is a flat COM-style image loaded at 0x100.  Its startup
    // reaches this loop after initialization; it repeatedly calls 0x10f05,
    // tests AL, treats 0x0b/0x0c separately, then converts 0x3b..0x44 to a
    // zero-based eight-byte dispatch-table index passed to 0x76f0.
    constexpr std::size_t load_bias = 0x100;
    constexpr std::size_t entry_offset = 0xd2b0 - load_bias;
    constexpr std::size_t loop_offset = 0xd3d2 - load_bias;
    constexpr std::array<std::uint8_t, 15> entry{
        0x0e, 0x1f, 0x0e, 0x07, 0x8c, 0xc8, 0x8e, 0xd0,
        0xb8, 0x00, 0xda, 0x89, 0xc4, 0xb8, 0x1f};
    constexpr auto loop = std::to_array<std::uint8_t>({
        0xe8, 0xe6, 0x3a, 0xe8, 0x29, 0xa2, 0xe8, 0xf0, 0xa7,
        0xe8, 0x27, 0x3b, 0x22, 0xc0, 0x74, 0xf0, 0x32, 0xe4,
        0x3c, 0x0b, 0x74,
        0x26, 0x8a, 0x0e, 0x3a, 0xda, 0x22, 0xc9, 0x75, 0xe2,
        0x3c, 0x0c, 0x75, 0x05, 0xe8, 0x79, 0x01, 0x33, 0xc0,
        0x2c, 0x3b, 0x3c, 0x0a, 0x73, 0xd3, 0xbe, 0xbf, 0x2f,
        0x32, 0xe4, 0xc0, 0xe0, 0x03, 0x01, 0xc6, 0xe8, 0xe4, 0xa2});
    if (!has_bytes(game_executable, entry_offset, entry)
        || !has_bytes(game_executable, loop_offset, loop)) {
        throw std::runtime_error("Unsupported Millennium DOS main-loop control flow");
    }
    return {
        .entry_address = 0xd2b0,
        .main_loop_address = 0xd3d2,
        .action_poll_address = 0x10f05,
        .special_action_0 = 0x0b,
        .special_action_1 = 0x0c,
        .function_key_first_action = 0x3b,
        .function_key_count = 10,
        .function_key_table_address = 0x2fbf,
        .function_key_table_stride = 8,
        .function_key_dispatch_address = 0x76f0,
    };
}

} // namespace eon
