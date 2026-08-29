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
    // AX=$06 is reached by TITLES.EXE's $1712 setup. These are entry-side
    // record offsets and local I/O facts only; callers must not emulate its
    // unproven ES:BX record ABI or resulting blit.
    std::uint16_t function_six_address = 0;
    // Both identified drivers load a far pointer from ES:BX + $00.  Its
    // ownership and pointed-to format remain outside this profile.
    std::uint16_t function_six_source_pointer_offset = 0;
    std::uint16_t function_six_source_pointer_load_address = 0;
    // These are raw accesses after the first far-pointer load, not a shared
    // source format.  EGA reads words at +$04, +$02 and +$00; MCGA reads the
    // +$02 word and then uses +$04 as another far pointer.
    std::uint16_t function_six_source_word_zero_read_address = 0;
    std::uint16_t function_six_source_word_two_read_address = 0;
    std::uint16_t function_six_source_word_four_read_address = 0;
    std::uint16_t function_six_source_nested_pointer_load_address = 0;
    std::uint16_t function_six_screen_width = 0;
    std::uint16_t function_six_horizontal_offset = 0;
    std::uint16_t function_six_height_offset = 0;
    std::uint16_t function_thirteen_address = 0;
    std::uint16_t function_thirteen_status_port = 0;
    std::uint8_t function_thirteen_retrace_mask = 0;
    std::uint16_t function_thirteen_first_poll_address = 0;
    std::uint16_t function_thirteen_second_poll_address = 0;
    std::uint16_t function_thirty_one_address = 0;
    std::uint16_t function_thirty_one_state_address = 0;
    std::uint8_t function_thirty_one_return_ah = 0;
};

[[nodiscard]] MillenniumDosVideoDriverProfile parse_millennium_dos_video_driver(
    std::span<const std::uint8_t> bytes, MillenniumDosVideoDriverKind kind);

// Spanish FAT12 drivers are accepted only by their own content identities;
// their matching dispatch offsets never authorize English-driver substitution.
[[nodiscard]] MillenniumDosVideoDriverProfile parse_millennium_dos_spanish_video_driver(
    std::span<const std::uint8_t> bytes, MillenniumDosVideoDriverKind kind);

} // namespace eon
