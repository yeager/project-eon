#pragma once

#include <cstddef>
#include <cstdint>
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

// Parses the fixed NUL-terminated celestial-label table from the original
// English static-data binary. This is intentionally strict: callers receive
// no fallback labels, translations, or generated data if original media is
// absent or differs from the currently verified release.
[[nodiscard]] MillenniumDosGameData parse_millennium_dos_game_data(
    std::span<const std::uint8_t> static_data);

} // namespace eon
