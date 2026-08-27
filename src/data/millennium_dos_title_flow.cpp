#include "data/millennium_dos_title_flow.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace eon {
namespace {

bool has_bytes(std::span<const std::uint8_t> bytes, std::size_t offset,
               std::span<const std::uint8_t> expected) {
    return offset <= bytes.size() && expected.size() <= bytes.size() - offset
        && std::equal(expected.begin(), expected.end(), bytes.begin() + static_cast<std::ptrdiff_t>(offset));
}

std::size_t require_unique(std::span<const std::uint8_t> bytes,
                           std::span<const std::uint8_t> expected,
                           const char* description) {
    std::size_t result = bytes.size();
    for (std::size_t offset = 0; offset + expected.size() <= bytes.size(); ++offset) {
        if (!has_bytes(bytes, offset, expected)) continue;
        if (result != bytes.size()) {
            throw std::runtime_error(std::string("Ambiguous Millennium DOS ") + description);
        }
        result = offset;
    }
    if (result == bytes.size()) {
        throw std::runtime_error(std::string("Missing Millennium DOS ") + description);
    }
    return result;
}

} // namespace

MillenniumDosTitleFlow parse_millennium_dos_title_flow(
    std::span<const std::uint8_t> titles_executable,
    std::span<const std::uint8_t> mill_launcher) {
    // Flat DOS files are loaded at offset 0x100.  The entry jump at file 0
    // lands at 0x1b80, where the title program establishes its stack.
    constexpr std::array<std::uint8_t, 7> entry_jump{
        0x0e, 0x1f, 0x0e, 0x07, 0xe9, 0x79, 0x1a};
    constexpr std::array<std::uint8_t, 6> title_selection{
        0xb8, 0x00, 0x00, 0xe8, 0x0b, 0xfb}; // AX=0; call 0x1725
    constexpr std::array<std::uint8_t, 13> transition_setup{
        0xb9, 0x25, 0x00, 0xba, 0x70, 0x01, 0x51, 0x52,
        0xbe, 0x0c, 0x01, 0x8b, 0x04};
    constexpr std::array<std::uint8_t, 7> input_poll{
        0xb4, 0x06, 0xb2, 0xff, 0xcd, 0x21, 0xc3};
    constexpr std::array<std::uint8_t, 11> input_branch{
        0xe8, 0xdf, 0xf0, 0x22, 0xc0, 0x75, 0x25, 0xb8,
        0x13, 0x00, 0xe8};
    constexpr std::array<std::uint8_t, 10> clean_exit{
        0x32, 0xc0, 0x2e, 0xa2, 0x0e, 0x1a, 0x8b, 0x26,
        0xa0, 0x1a};
    constexpr std::array<std::uint8_t, 8> dos_exit{
        0x2e, 0xa0, 0x0e, 0x1a, 0xb4, 0x4c, 0xcd, 0x21};

    if (!has_bytes(titles_executable, 0, entry_jump)) {
        throw std::runtime_error("Unsupported Millennium DOS title entry");
    }
    constexpr std::size_t file_to_load_bias = 0x100;
    constexpr std::size_t title_selection_offset = 0x1c14 - file_to_load_bias;
    constexpr std::size_t input_branch_offset = 0x1c28 - file_to_load_bias;
    constexpr std::size_t clean_exit_offset = 0x1c5a - file_to_load_bias;
    constexpr std::size_t dos_exit_offset = 0x1a12 - file_to_load_bias;
    if (!has_bytes(titles_executable, title_selection_offset, title_selection)
        || !has_bytes(titles_executable, input_branch_offset, input_branch)
        || !has_bytes(titles_executable, clean_exit_offset, clean_exit)
        || !has_bytes(titles_executable, dos_exit_offset, dos_exit)) {
        throw std::runtime_error("Unsupported Millennium DOS title control flow");
    }
    static_cast<void>(require_unique(titles_executable, transition_setup, "title transition loop"));
    static_cast<void>(require_unique(titles_executable, input_poll, "title input poll"));

    // MILL.COM calls its EXEC wrapper at 0x31c first with the title string
    // and, only after it returns, with the game executable string.
    constexpr std::array<std::uint8_t, 14> launcher_sequence{
        0xba, 0x8f, 0x06, 0xe8, 0xd9, 0x00, 0x22, 0xc0,
        0x75, 0x19, 0x0e, 0x1f, 0xba, 0x9a};
    constexpr std::array<std::uint8_t, 11> title_name{
        'T', 'I', 'T', 'L', 'E', 'S', '.', 'E', 'X', 'E', 0};
    constexpr std::array<std::uint8_t, 11> game_name{
        '2', '2', '0', '0', 'a', 'd', '.', 'e', 'x', 'e', 0};
    static_cast<void>(require_unique(mill_launcher, launcher_sequence, "launcher sequence"));
    const auto title_offset = require_unique(mill_launcher, title_name, "launcher title program");
    const auto game_offset = require_unique(mill_launcher, game_name, "launcher game program");
    if (title_offset >= game_offset || game_offset != title_offset + title_name.size()) {
        throw std::runtime_error("Invalid Millennium DOS launcher hand-off order");
    }

    return {
        .title_entry_address = 0x1b80,
        .title_resource_index = 0,
        .intro_transition_steps = 0x25,
        .intro_step_stride = 0x170,
        .input_interrupt = 0x21,
        .input_service = 0x06,
        .input_parameter = 0xff,
        .exit_code = 0,
        .launcher_title_offset = title_offset,
        .launcher_game_offset = game_offset,
        .launcher_title_program = "TITLES.EXE",
        .launcher_game_program = "2200ad.exe",
    };
}

} // namespace eon
