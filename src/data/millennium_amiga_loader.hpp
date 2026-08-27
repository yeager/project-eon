#pragma once

#include "data/amiga_adf.hpp"

#include <array>
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

// The next complete subroutine in the raw resident range is a literal
// sign/magnitude splitter.  It reads three words from A1+$36, writes their
// low fifteen bits and former high bits to fixed RAM locations, then calls a
// resident helper.  Names here describe data movement only, not gameplay
// meaning.  Project Eon does not execute this routine.
struct MillenniumAmigaResidentWordSplitter {
    std::uint32_t entry_address = 0;
    std::uint16_t source_a1_offset = 0;
    std::array<std::uint32_t, 3> magnitude_word_addresses{};
    std::array<std::uint32_t, 3> sign_byte_addresses{};
    std::uint32_t helper_address = 0;
    std::uint32_t signed_word_address = 0;
    std::uint32_t signed_sign_address = 0;
};

// Exact observable state immediately before the splitter's JSR $7ba12.  This
// is intentionally not presented as the subroutine's final result: the helper
// is not yet independently decoded, and may alter the fixed output locations.
struct MillenniumAmigaResidentWordSplitterPreHelperState {
    std::array<std::uint16_t, 3> magnitude_words{};
    std::array<std::uint8_t, 3> sign_bytes{};
};

// Chain-of-custody evidence for the location named by the splitter's JSR.
// The raw ADF is mapped directly at `resident_stage.destination` for the
// purpose of locating its source bytes, but those bytes have not established
// a complete executable helper boundary. This record intentionally exposes
// only the literal mapping and a small immutable fingerprint; it must not be
// used as an implementation of the helper.
struct MillenniumAmigaResidentHelperRawBoundary {
    std::uint32_t helper_address = 0;
    std::uint32_t raw_disk_offset = 0;
    std::array<std::uint8_t, 32> raw_prefix{};
    std::string raw_prefix_sha256;
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

// Validates the next, independent resident subroutine after the entry gate.
// It is a read-only byte profile of the supplied raw ADF; no transformed first
// stage, disk extraction, or 68000 execution is involved.
[[nodiscard]] MillenniumAmigaResidentWordSplitter parse_millennium_amiga_resident_word_splitter(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan);

// Models only the three LSL/ROXL/LSR iterations proven by the validated raw
// routine. `source_words` are the big-endian words already resident at
// A1+$36..+$3b. It performs no disk I/O and cannot stand in for the later
// helper call or the routine's return value.
[[nodiscard]] MillenniumAmigaResidentWordSplitterPreHelperState
split_millennium_amiga_resident_words_pre_helper(
    const std::array<std::uint16_t, 3>& source_words);

// Locates and fingerprints the raw media bytes corresponding to the
// splitter's `$7ba12` target. It fails closed unless the source image has the
// verified shared prefix. It neither executes, transforms, nor assigns a
// routine boundary to those bytes.
[[nodiscard]] MillenniumAmigaResidentHelperRawBoundary
parse_millennium_amiga_resident_helper_raw_boundary(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan,
    const MillenniumAmigaResidentWordSplitter& splitter);

} // namespace eon
