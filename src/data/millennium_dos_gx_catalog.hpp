#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace eon {

// One immutable resource record in the verified English DOS GX.LIB catalogue.
// Names and hashes describe source evidence only; they do not assign
// UI/gameplay semantics or make an image drawable by a runtime path.
struct MillenniumDosGxBitmapRecord {
    std::string name;
    std::uint32_t source_offset = 0;
    std::uint32_t source_size = 0;
    std::string source_sha256;
    std::uint8_t flags = 0;
    std::uint8_t max_palette_index = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint16_t encoded_span = 0;
    bool bitmap_decoder_admitted = false;
    // A bounded decoder rejection is a preservation boundary, not a fallback
    // image format or a reason to reinterpret the original resource.
    std::string decoder_boundary;
    std::string decoded_pixels_sha256;
};

// Bounded, all-resource inventory for the supplied English DOS GX.LIB.
// The decoder reads the user archive in memory and discards decoded pixels
// after hashing them, so it never extracts, caches, or mutates original data.
struct MillenniumDosGxBitmapCatalog {
    std::string source_sha256;
    std::size_t resource_count = 0;
    std::size_t bitmap_decoder_admitted_count = 0;
    std::size_t bitmap_decoder_boundary_count = 0;
    std::uint64_t decoded_pixel_count = 0;
    std::vector<MillenniumDosGxBitmapRecord> resources;
};

[[nodiscard]] MillenniumDosGxBitmapCatalog inspect_millennium_dos_gx_bitmap_catalog(
    std::span<const std::uint8_t> gx_lib);

} // namespace eon
