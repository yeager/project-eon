#pragma once

#include "data/deuteros_amiga_loader.hpp"
#include "data/deuteros_amiga_frame.hpp"

#include <cstdint>
#include <vector>

namespace eon {

// A strictly bounded observation of the $20580 byte-stream interpreter. It
// retains original byte codes and pointer arithmetic, rather than turning the
// stream into a replacement bitmap or a semantic UI string.
struct DeuterosAmigaAlternateRendererTrace {
    std::uint32_t stream_address = 0;
    std::uint32_t stream_offset = 0;
    std::uint8_t position_column = 0;
    std::uint8_t position_row = 0;
    std::uint32_t primary_video_offset = 0;
    std::uint8_t primary_table_selector = 0;
    std::uint8_t secondary_table_selector = 0;
    std::vector<std::uint8_t> glyph_codes;
};

// Interpret only the exact command classes observed in the supplied opening
// stream: $16 (position), $10/$11 (two original table selectors), printable
// glyph bytes, and $00 (return). `stream_address` must address the supplied
// $21932 payload at its original $32a24 destination. Any other command is a
// preservation boundary and is rejected instead of being guessed or drawn.
[[nodiscard]] DeuterosAmigaAlternateRendererTrace
trace_deuteros_amiga_alternate_renderer(
    const DeuterosAmigaMainResourceTransfer& transfer,
    const DeuterosAmigaMainStageEntry& entry, std::uint32_t stream_address);

// Apply the fully recovered bitplane writes for one already-traced $20580
// stream.  The 8x8 glyph rows, selector tables, display byte geometry and
// per-plane write formula are all read from the original main-stage image.
// This mutates only the in-memory presentation frame; it never alters media
// or substitutes a host font.  Unknown global layouts and out-of-plane writes
// are rejected instead of being clipped or guessed.
void apply_deuteros_amiga_alternate_renderer(
    DeuterosAmigaFrame& frame, const AmigaAdf& disk,
    const DeuterosAmigaLoadPlan& plan,
    const DeuterosAmigaAlternateRendererTrace& trace);

} // namespace eon
