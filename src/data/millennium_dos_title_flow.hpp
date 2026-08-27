#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace eon {

// Facts extracted from the supplied English DOS title executable and its
// MILL.COM launcher.  This deliberately models the verified hand-off only;
// it is not a replacement implementation of TITLES.EXE's renderer.
struct MillenniumDosTitleFlow {
    std::uint16_t title_entry_address = 0;
    std::uint16_t title_resource_index = 0;
    std::uint16_t intro_transition_steps = 0;
    std::uint16_t intro_step_stride = 0;
    std::uint8_t input_interrupt = 0;
    std::uint8_t input_service = 0;
    std::uint8_t input_parameter = 0;
    std::uint8_t exit_code = 0;
    std::size_t launcher_title_offset = 0;
    std::size_t launcher_game_offset = 0;
    std::string launcher_title_program;
    std::string launcher_game_program;
};

// Validates code paths rather than trusting names or offsets alone.  The
// present evidence covers the supplied English DOS executables only; callers
// must not apply its behaviour to another release without fresh evidence.
[[nodiscard]] MillenniumDosTitleFlow parse_millennium_dos_title_flow(
    std::span<const std::uint8_t> titles_executable,
    std::span<const std::uint8_t> mill_launcher);

} // namespace eon
