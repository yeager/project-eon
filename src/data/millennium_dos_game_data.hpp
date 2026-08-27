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

// Parses the exact, read-only save serialization established by the English
// DOS executable's load path.  It does not infer game rules nor mutate or
// unpack original media.  Unknown save regions remain intentionally opaque.
[[nodiscard]] MillenniumDosSaveLayout parse_millennium_dos_save_layout(
    std::span<const std::uint8_t> save_data);

} // namespace eon
