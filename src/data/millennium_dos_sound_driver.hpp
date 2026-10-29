#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <span>
#include <string>
#include <string_view>

namespace eon {

// These values are the three literal menu choices accepted by MILL.COM's
// recovered routine.  They describe its source-level selection only; Eon
// does not poll for a choice or attempt to initialize an original driver.
enum class MillenniumDosSoundEffectChoice : std::uint8_t {
    ibm_speaker = 0,
    sound_blaster = 1,
    covox_sound_master = 2,
};

// Exact sound-driver leaves supplied by the English DOS release.  Admission
// is content-addressed and does not imply that the driver's ABI is recovered.
enum class MillenniumDosSoundDriverKind { ibm_speaker, sound_blaster, covox_sound_master };

struct MillenniumDosSoundDriverLeaf {
    MillenniumDosSoundDriverKind kind{};
    std::string_view original_filename;
    // The admission parser computes this digest locally, so the returned
    // evidence must own it rather than retaining a dangling view.
    std::string sha256;
    std::size_t byte_size = 0;
};

struct MillenniumDosSoundSelectionEvidence {
    std::string_view launcher_sha256;
    std::uint16_t selector_entry_address = 0;
    std::size_t selector_byte_count = 0;
    std::string_view selector_sha256;
    std::uint16_t prompt_address = 0;
    std::size_t prompt_byte_count = 0;
    std::string_view prompt_sha256;
    std::uint16_t filename_table_address = 0;
    std::size_t filename_table_byte_count = 0;
    std::string_view filename_table_sha256;
    std::uint16_t selection_table_address = 0;
    std::size_t selection_table_byte_count = 0;
    std::string_view selection_table_sha256;
    // Values 0, 1, and 2 select these original table slots: 0, 3, and 4.
    std::uint8_t ibm_speaker_table_slot = 0;
    std::uint8_t sound_blaster_table_slot = 0;
    std::uint8_t covox_table_slot = 0;
    std::string_view ibm_speaker_filename;
    std::string_view sound_blaster_filename;
    std::string_view covox_filename;
    // This literal table entry is present in MILL.COM but no SROL.DRV leaf
    // exists in the supplied archive. It is a preservation boundary, never a
    // fallback candidate.
    std::uint8_t missing_srol_table_slot = 0;
    std::string_view missing_srol_filename;
};

// Validate the exact English MILL.COM selection routine and its literal file
// tables. This parser admits no guessed hardware policy or interactive input.
[[nodiscard]] MillenniumDosSoundSelectionEvidence
parse_millennium_dos_sound_selection(std::span<const std::uint8_t> mill_com);

// Return the byte-locked, DOS `$`-terminated sound-selection prompt from the
// supplied launcher without translating, normalising, or retaining it. The
// caller owns the returned in-memory view of user-supplied original text.
[[nodiscard]] std::string extract_millennium_dos_sound_selection_prompt(
    std::span<const std::uint8_t> mill_com,
    const MillenniumDosSoundSelectionEvidence& evidence);

// Admit only the three selectable sound-driver leaves supplied by the English
// release, by complete content identity. SROL.DRV and other releases remain
// rejected.
[[nodiscard]] MillenniumDosSoundDriverLeaf
admit_millennium_dos_sound_driver_leaf(std::span<const std::uint8_t> bytes);

// Exact SIBM calls made by the English 2200AD startup helper at $d07a. The
// profile records dispatch identities and stack-independent literal inputs;
// it does not execute the private interrupt or infer driver return values.
struct MillenniumDosSibmStartupProfile {
    std::array<std::uint16_t, 17> dispatch_targets{};
    std::array<std::uint16_t, 14> function_eleven_name_addresses{};
    std::array<std::string, 14> function_eleven_filenames{};
    std::uint16_t function_zero_target = 0;
    std::uint16_t function_eleven_target = 0;
    std::uint16_t function_twelve_target = 0;
    std::uint16_t startup_helper_address = 0;
    std::uint16_t first_private_interrupt_address = 0;
    std::uint16_t function_zero_wrapper_address = 0;
    std::uint16_t function_twelve_wrapper_address = 0;
    std::uint16_t function_eleven_wrapper_address = 0;
    std::uint16_t function_eleven_argument_table = 0;
    std::uint16_t function_twelve_requested_bytes = 0;
    std::uint16_t function_eleven_first_argument = 0;
    std::uint16_t function_eleven_last_argument = 0;
    std::uint8_t function_eleven_call_count = 0;
    std::uint16_t function_zero_input_for_mode_one = 0;
    std::uint16_t function_zero_input_for_other_modes = 0;
    std::string driver_sha256;
    std::string startup_helper_sha256;
    std::string function_eleven_argument_table_sha256;
};

[[nodiscard]] MillenniumDosSibmStartupProfile
parse_millennium_dos_sibm_startup_profile(
    std::span<const std::uint8_t> game_executable,
    std::span<const std::uint8_t> sibm_driver);

} // namespace eon
