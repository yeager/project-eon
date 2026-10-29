#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

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
    std::uint16_t function_zero_cached_mode_store_instruction = 0;
    std::uint16_t function_zero_cached_mode_known_branch_target = 0;
    std::uint16_t function_four_address = 0;
    std::uint8_t function_zero_video_mode = 0;
    std::uint16_t function_zero_set_mode_interrupt_site = 0;
    std::uint16_t function_zero_verify_mode_interrupt_site = 0;
    std::uint16_t function_zero_mode_match_branch_target = 0;
    std::uint16_t function_zero_mode_mismatch_return = 0;
    std::uint16_t function_zero_mode_mismatch_zero_ax_instruction = 0;
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
    std::uint16_t function_six_width_offset = 0;
    std::uint16_t function_thirteen_address = 0;
    std::uint16_t function_thirteen_status_port = 0;
    std::uint8_t function_thirteen_retrace_mask = 0;
    std::uint16_t function_thirteen_first_poll_address = 0;
    std::uint16_t function_thirteen_second_poll_address = 0;
    std::uint16_t function_thirty_one_address = 0;
    std::uint16_t function_thirty_one_state_address = 0;
    std::uint8_t function_thirty_one_return_ah = 0;
    std::uint16_t function_nineteen_address = 0;
};

// The English EGA driver's function-$19 slot is a hash-verified one-byte RET.
// This bounded result covers the exact common handler prefix and wrapper
// return, without claiming any BIOS, device, or graphics effect.
struct MillenniumDosEgaFunctionNineteenReturn {
    std::uint16_t driver_entry_address = 0;
    std::uint16_t handler_dispatch_offset = 0;
    std::uint16_t handler_return_ax = 0;
    bool caller_flags_restored_unknown = true;
};

// Exact English MCGA function-$19 startup path for TITLES.EXE. The routine
// clears 255 RGB DAC entries; Eon's caller may abstract only VGA retrace wait
// durations, never the observed beam position. No destination pointer is used
// on the admitted zero-$09c7 path.
struct MillenniumDosMcgaFunctionNineteenReturn {
    std::uint16_t driver_entry_address = 0x09ee;
    std::uint16_t handler_dispatch_offset = 0x0032;
    std::uint16_t handler_return_ax = 0x0100;
    std::uint8_t first_dac_index = 0;
    std::uint16_t dac_entry_count = 0x00ff;
    std::uint8_t dac_component_value = 0;
    bool caller_flags_restored_unknown = true;
    bool retrace_wait_durations_abstracted_by_eon = true;
};

// Literal result of MCGA function-$1a for the admitted first title request.
// One element is one complete RGB DAC entry in the order in which the handler
// writes it. `phase` is the handler's outer pass, rather than host time.
struct MillenniumDosMcgaFunctionTwentySixDacWrite {
    std::uint8_t phase = 0;
    std::uint8_t register_index = 0;
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
    constexpr bool operator==(const MillenniumDosMcgaFunctionTwentySixDacWrite&) const = default;
};

struct MillenniumDosMcgaFunctionTwentySixReturn {
    std::uint16_t driver_entry_address = 0x0b67;
    std::uint16_t handler_dispatch_offset = 0x0032;
    std::uint8_t phase_count = 25;
    std::vector<MillenniumDosMcgaFunctionTwentySixDacWrite> dac_writes;
    bool caller_flags_restored_unknown = true;
    bool retrace_wait_durations_abstracted_by_eon = true;
};

[[nodiscard]] MillenniumDosVideoDriverProfile parse_millennium_dos_video_driver(
    std::span<const std::uint8_t> bytes, MillenniumDosVideoDriverKind kind);

// Spanish FAT12 drivers are accepted only by their own content identities;
// their matching dispatch offsets never authorize English-driver substitution.
[[nodiscard]] MillenniumDosVideoDriverProfile parse_millennium_dos_spanish_video_driver(
    std::span<const std::uint8_t> bytes, MillenniumDosVideoDriverKind kind);

[[nodiscard]] std::optional<MillenniumDosEgaFunctionNineteenReturn>
execute_millennium_dos_ega_function_nineteen(
    std::span<const std::uint8_t> exact_driver,
    std::span<const std::uint8_t> loaded_driver,
    std::uint16_t input_ax);

[[nodiscard]] std::optional<MillenniumDosMcgaFunctionNineteenReturn>
execute_millennium_dos_mcga_function_nineteen(
    std::span<const std::uint8_t> exact_driver,
    std::span<const std::uint8_t> loaded_driver,
    std::span<const std::uint8_t> exact_titles,
    std::span<const std::uint8_t> call_record,
    std::span<const std::uint8_t> loaded_palette_source,
    std::uint16_t child_code_segment,
    std::uint16_t entry_stack_pointer,
    std::uint16_t input_ax,
    std::uint8_t live_compare_flag_09c7,
    std::uint8_t live_dispatch_flag_01e5);

// Static translation of the admitted zero-$09c7 branch of MCGA function-$1a.
// It returns every DAC entry in original order. The caller decides whether an
// Eon presentation adapter presents the intermediate passes synchronously.
[[nodiscard]] std::optional<MillenniumDosMcgaFunctionTwentySixReturn>
execute_millennium_dos_mcga_function_twenty_six(
    std::span<const std::uint8_t> exact_driver,
    std::span<const std::uint8_t> loaded_driver,
    std::span<const std::uint8_t> call_record,
    std::span<const std::uint8_t> loaded_palette_source,
    std::uint16_t child_code_segment,
    std::uint8_t live_copy_flag_09c7);

// Isolated source probe for locating the next MCGA startup boundary. It
// applies one explicitly synthetic Eon policy byte to the hash-locked local
// store model; it never advances a live function-zero session or writes guest
// memory. Only AL is synthesized because the source consumes no other result
// from the first AH=$0f query.
struct MillenniumDosMcgaVideoProbe {
    char startup_input = '0';
    std::uint16_t source_query_address = 0;
    std::uint8_t source_interrupt = 0x10;
    std::uint8_t source_function = 0x0f;
    std::uint8_t synthetic_al = 0;
    std::uint16_t hypothetical_store_instruction = 0;
    std::uint16_t hypothetical_driver_offset = 0;
    std::uint16_t next_interrupt_address = 0;
    std::uint8_t next_interrupt = 0x10;
    std::uint16_t next_ax = 0;
    std::uint16_t next_ax_known_mask = 0;
};

[[nodiscard]] MillenniumDosMcgaVideoProbe evaluate_millennium_dos_mcga_video_probe(
    std::span<const std::uint8_t> exact_mcga_driver, char startup_input);

} // namespace eon
