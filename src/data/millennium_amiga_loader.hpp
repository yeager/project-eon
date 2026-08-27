#pragma once

#include "data/amiga_adf.hpp"

#include <cstdint>
#include <string>

namespace eon {

// The supplied Millennium 2.2 Amiga images carry their game data in raw disk
// ranges.  This is a description of the requests made by the on-disk loader,
// not an extracted or installed representation of the game.
struct MillenniumAmigaLoadStage {
    std::uint32_t disk_offset = 0;
    std::uint32_t length = 0;
    std::uint32_t destination = 0;
    // SHA-256 of precisely the bytes requested from the supplied ADF. This is
    // evidence only; Project Eon never materializes this range as a file.
    std::string raw_sha256;
};

struct MillenniumAmigaLoadPlan {
    MillenniumAmigaLoadStage bootstrap_loader;
    MillenniumAmigaLoadStage first_stage;
    MillenniumAmigaLoadStage resident_stage;
    std::uint32_t resident_entry = 0;
    std::uint32_t loader_magic = 0;
};

// The resident range begins with a small 68000 entry gate.  It calls into the
// first loaded RAM stage and commits its returned word to a fixed location.
// This describes only the literal gate; the called stage is transformed by
// the preceding loader and is intentionally not guessed from raw disk bytes.
struct MillenniumAmigaResidentEntry {
    std::uint32_t entry_address = 0;
    std::uint32_t initializer_address = 0;
    std::uint32_t result_word_address = 0;
    std::uint16_t d3_nonzero_or_mask = 0;
};

// Recovers the explicit raw-read requests from the first-stage 68000 loader.
// It validates the instruction sequence and every resulting disk range.  It
// intentionally does not decompress, write, or otherwise unpack game media.
[[nodiscard]] MillenniumAmigaLoadPlan parse_millennium_amiga_load_plan(
    const AmigaAdf& disk);

// Decode the exact first resident instructions after the verified raw-loader
// handoff. This is a read-only control-flow profile, not a decoder for the
// transformed first-stage RAM image.
[[nodiscard]] MillenniumAmigaResidentEntry parse_millennium_amiga_resident_entry(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan);

} // namespace eon
