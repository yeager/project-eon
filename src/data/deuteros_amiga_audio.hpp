#pragma once

#include "data/deuteros_amiga_bundle.hpp"

#include <cstdint>
#include <vector>

namespace eon {

// A single 14-byte entry consumed by the original Paula setup routine at
// $22ab8. `pcm` retains the exact DMA bytes (signed 8-bit samples on Paula);
// no file is unpacked or rewritten by this reader.
struct DeuterosAmigaSound {
    std::uint32_t sample_relative_offset = 0;
    std::uint16_t length_words = 0;
    std::uint16_t period = 0;
    std::uint16_t volume = 0;
    std::uint16_t control_word = 0;
    std::uint16_t parameter_word = 0;
    std::vector<std::uint8_t> pcm;
};

struct DeuterosAmigaSoundBank {
    std::uint32_t table_relative_offset = 0;
    std::vector<DeuterosAmigaSound> sounds;
    // Bytes before the next auxiliary object that are not reachable through
    // the routine's 14-byte stride. Kept verbatim rather than guessed away.
    std::vector<std::uint8_t> trailing_bytes;
};

// Parses the raw, bundle-relative table installed into $22aa6 by $212d0.
// The following auxiliary pointer is the proven end boundary.  It is not a
// container extraction format: every returned byte is read directly from the
// supplied original ADF image.
[[nodiscard]] DeuterosAmigaSoundBank parse_deuteros_amiga_sound_bank(
    const AmigaAdf& disk, const DeuterosAmigaBundle& bundle);

} // namespace eon
