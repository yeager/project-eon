#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <span>
#include <string>
#include <vector>

namespace eon {

// Immutable labels recovered from the supplied English DOS 2200AD4.BIN.
// They are deliberately kept separate from mutable game state: the binary
// establishes their byte layout and order, but does not by itself prove the
// simulation fields that later refer to them.
struct MillenniumDosCelestialLabel {
    std::size_t source_offset = 0;
    std::string text;
};

struct MillenniumDosGameData {
    std::size_t celestial_table_offset = 0;
    std::vector<MillenniumDosCelestialLabel> celestial_labels;
};

// `2200AD4.BIN` begins with a 0x366-byte array of 16-bit offsets.  It is a
// native pointer table into the rest of the same static-data file, rather
// than a replacement UI description.  Several pointers intentionally share
// a record and one pair is not in ascending target order, so preserve the
// table order separately from the deduplicated raw record extents.
struct MillenniumDosStaticTextPointer {
    std::size_t table_index = 0;
    std::size_t target_offset = 0;
};

struct MillenniumDosStaticTextRecord {
    std::size_t source_offset = 0;
    std::vector<std::uint8_t> bytes;
};

struct MillenniumDosStaticTextCatalog {
    static constexpr std::size_t pointer_table_size = 0x366;
    static constexpr std::size_t pointer_count = pointer_table_size / 2;

    std::vector<MillenniumDosStaticTextPointer> pointers;
    std::vector<MillenniumDosStaticTextRecord> records;
};

// The supplied Spanish FAT12 release contains the only live standalone
// launcher documentation in the recognised corpus: MILL.BAT. Its text is
// preserved verbatim after identity validation; it establishes command-tail
// launch choices, not gameplay controls or host-side driver policy.
struct MillenniumDosLaunchManual {
    std::string sha256;
    std::string original_text;
};

// The save/load routines in the verified English 2200AD.EXE transfer this
// four-word subset in columns, not as a packed file record.  The executable
// reconstructs each element at a 28-byte stride in its runtime table.  Names
// deliberately describe their recovered positions, rather than guessing what
// a particular value means to the simulation.
struct MillenniumDosStateTableEntry {
    std::uint16_t runtime_offset_0 = 0;
    std::uint16_t runtime_offset_4 = 0;
    std::uint16_t runtime_offset_6 = 0;
    std::uint16_t runtime_offset_8 = 0;
};

struct MillenniumDosSaveLayout {
    static constexpr std::size_t version_offset = 0x0000;
    static constexpr std::size_t version_size = 2;
    static constexpr std::size_t state_table_count = 38;
    static constexpr std::size_t state_table_runtime_stride = 0x1c;
    static constexpr std::size_t state_table_offset_6_column = 0x20ab;
    static constexpr std::size_t state_table_offset_0_and_4_columns = 0x245e;
    static constexpr std::size_t state_table_offset_8_column = 0x24f6;
    static constexpr std::size_t serialized_size = 0x2542;
    static constexpr std::uint16_t expected_version = 0x0056;

    std::uint16_t version = 0;
    std::array<MillenniumDosStateTableEntry, state_table_count> state_table{};
};

// Parses the fixed NUL-terminated celestial-label table from the original
// English static-data binary. This is intentionally strict: callers receive
// no fallback labels, translations, or generated data if original media is
// absent or differs from the currently verified release.
[[nodiscard]] MillenniumDosGameData parse_millennium_dos_game_data(
    std::span<const std::uint8_t> static_data);

// Preserves the full pointer-to-raw-record topology of the original static
// data. No text is normalized, decoded as a UI command, or assigned gameplay
// meaning; callers can only inspect bytes read from supplied media.
[[nodiscard]] MillenniumDosStaticTextCatalog parse_millennium_dos_static_text_catalog(
    std::span<const std::uint8_t> static_data);

[[nodiscard]] MillenniumDosLaunchManual parse_millennium_dos_spanish_launch_manual(
    std::span<const std::uint8_t> mill_bat);

// Parses the exact, read-only save serialization established by the English
// DOS executable's load path.  It does not infer game rules nor mutate or
// unpack original media.  Unknown save regions remain intentionally opaque.
[[nodiscard]] MillenniumDosSaveLayout parse_millennium_dos_save_layout(
    std::span<const std::uint8_t> save_data);

} // namespace eon
