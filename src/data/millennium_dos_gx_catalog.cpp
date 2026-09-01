#include "data/millennium_dos_gx_catalog.hpp"

#include "data/millennium_dos_bitmap.hpp"
#include "data/millennium_dos_lib.hpp"
#include "data/sha256.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace eon {

MillenniumDosGxBitmapCatalog inspect_millennium_dos_gx_bitmap_catalog(
    const std::span<const std::uint8_t> gx_lib) {
    constexpr std::string_view english_gx_library_sha256 =
        "4adf9991226deab4749ac07ad637851994f57d11f6dc45f3f5ce862b5bc34c2f";
    if (to_hex(sha256(gx_lib)) != english_gx_library_sha256) {
        throw std::runtime_error("Unsupported Millennium English DOS GX library");
    }

    const MillenniumDosLib library({gx_lib.begin(), gx_lib.end()});
    MillenniumDosGxBitmapCatalog result;
    result.source_sha256 = std::string(english_gx_library_sha256);
    result.resource_count = library.entries().size();
    result.resources.reserve(result.resource_count);
    for (const auto& entry : library.entries()) {
        const auto bytes = library.read(entry);
        MillenniumDosGxBitmapRecord record;
        record.name = entry.name;
        record.source_offset = entry.offset;
        record.source_size = entry.size;
        record.source_sha256 = to_hex(sha256(bytes));
        try {
            const auto bitmap = decode_millennium_dos_bitmap(bytes);
            const auto pixel_count = static_cast<std::uint64_t>(bitmap.pixels.size());
            if (pixel_count > std::numeric_limits<std::uint64_t>::max() - result.decoded_pixel_count) {
                throw std::runtime_error("Millennium DOS GX decoded pixel count overflow");
            }
            result.decoded_pixel_count += pixel_count;
            record.flags = bitmap.flags;
            record.max_palette_index = bitmap.max_palette_index;
            record.width = bitmap.width;
            record.height = bitmap.height;
            record.encoded_span = bitmap.encoded_span;
            record.bitmap_decoder_admitted = true;
            record.decoded_pixels_sha256 = to_hex(sha256(bitmap.pixels));
            ++result.bitmap_decoder_admitted_count;
        } catch (const std::runtime_error& error) {
            record.decoder_boundary = error.what();
            ++result.bitmap_decoder_boundary_count;
        }
        result.resources.push_back(std::move(record));
    }
    return result;
}

} // namespace eon
