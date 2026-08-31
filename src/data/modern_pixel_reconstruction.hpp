#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace eon {

// A transient, deterministic Modern renderer operation over an already
// decoded original RGBA surface.  It deliberately has no file or cache API:
// callers retain the verified source bytes and may discard this derived buffer
// after uploading it to the renderer.
struct ModernReconstructedSurface {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
};

// Edge-aware Scale2x for a tightly packed RGBA32 image.  Unlike texture
// filtering, the four output pixels are selected from the original pixel
// neighbourhood, preserving hard pixel-art edges.  Throws if the source
// dimensions or byte span are malformed rather than guessing a layout.
ModernReconstructedSurface reconstruct_rgba_scale2x(
    std::span<const std::uint8_t> original_rgba, int width, int height);

// Edge-aware Scale4x, defined as two bounded Scale2x passes. Each output
// pixel is consequently selected from the supplied decoded source surface;
// this produces no files, cache entries, colours, game state, or media edits.
ModernReconstructedSurface reconstruct_rgba_scale4x(
    std::span<const std::uint8_t> original_rgba, int width, int height);

} // namespace eon
