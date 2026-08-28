#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace eon {

enum class MillenniumDosVideoDriverKind { ega640, mcga };

// Strict instruction-level profile of a private INT 91h driver loaded by
// MILL.COM at DS:0000. This is an adapter boundary, not a DOS/BIOS emulator.
struct MillenniumDosVideoDriverProfile {
    MillenniumDosVideoDriverKind kind{};
    std::size_t byte_size = 0;
    std::uint16_t dispatch_table_address = 0;
    std::uint16_t function_zero_address = 0;
    std::uint16_t function_four_address = 0;
    std::uint8_t function_zero_video_mode = 0;
    std::uint16_t function_zero_set_mode_interrupt_site = 0;
    std::uint16_t function_zero_mode_mismatch_return = 0;
    std::uint16_t function_four_input_offset = 0;
    std::uint8_t function_four_input_mask = 0;
    std::uint16_t function_four_state_address = 0;
    std::uint16_t function_thirty_one_address = 0;
    std::uint16_t function_thirty_one_state_address = 0;
    std::uint8_t function_thirty_one_return_ah = 0;
};

[[nodiscard]] MillenniumDosVideoDriverProfile parse_millennium_dos_video_driver(
    std::span<const std::uint8_t> bytes, MillenniumDosVideoDriverKind kind);

} // namespace eon
