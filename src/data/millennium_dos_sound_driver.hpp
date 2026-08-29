#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
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

// The supplied English release has admissible original leaves only for the
// two external choices below.  IBM speaker is selected in MILL.COM but has no
// admitted driver leaf here; SROL is a referenced, missing table leaf.
enum class MillenniumDosSoundDriverKind { sound_blaster, covox_sound_master };

struct MillenniumDosSoundDriverLeaf {
    MillenniumDosSoundDriverKind kind{};
    std::string_view original_filename;
    std::string_view sha256;
    std::size_t byte_size = 0;
};

struct MillenniumDosSoundSelectionEvidence {
    std::string_view launcher_sha256;
    std::uint16_t selector_entry_address = 0;
    std::size_t selector_byte_count = 0;
    std::string_view selector_sha256;
    std::uint16_t prompt_address = 0;
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

// Admit only the two supplied external sound-driver leaves by complete content
// identity. A matching filename, SIBM.DRV, SROL.DRV, or another release is not
// sufficient and is deliberately rejected.
[[nodiscard]] MillenniumDosSoundDriverLeaf
admit_millennium_dos_sound_driver_leaf(std::span<const std::uint8_t> bytes);

} // namespace eon
