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
    // Function $00 reads the low byte of the caller-owned ES:BX record, but
    // its verified local prefix does not branch on that byte. The remaining
    // facts identify only code-local cache handling and BIOS call sites.
    std::uint16_t function_zero_input_offset = 0;
    std::uint16_t function_zero_cached_mode_address = 0;
    std::uint8_t function_zero_cached_mode_unknown_sentinel = 0;
    std::uint16_t function_zero_cached_mode_query_interrupt_site = 0;
    std::uint16_t function_zero_cached_mode_unknown_branch_target = 0;
    std::uint16_t function_four_address = 0;
    std::uint8_t function_zero_video_mode = 0;
    std::uint16_t function_zero_set_mode_interrupt_site = 0;
    std::uint16_t function_zero_verify_mode_interrupt_site = 0;
    std::uint16_t function_zero_mode_match_return = 0;
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
