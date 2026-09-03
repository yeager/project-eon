#include "data/millennium_dos_last_screen.hpp"

#include "data/millennium_dos_lib.hpp"
#include "data/sha256.hpp"

#include <stdexcept>
#include <string_view>

namespace eon {

MillenniumDosLastScreen parse_millennium_dos_last_screen(
    const std::span<const std::uint8_t> last_lib) {
    constexpr std::string_view english_last_library_sha256 =
        "a3f5c0b447795881dd4cd5316a091ecc218b1bf563f567b6fe3f093f89781510";
    if (to_hex(sha256(last_lib)) != english_last_library_sha256) {
        throw std::runtime_error("Unsupported Millennium English DOS LAST library");
    }

    const MillenniumDosLib library(last_lib);
    if (library.entries().size() != 1 || library.entries().front().name != "last") {
        throw std::runtime_error("Unsupported Millennium DOS LAST.LIB directory");
    }

    const auto resource = library.read(library.entries().front());
    MillenniumDosLastScreen result;
    result.bitmap = decode_millennium_dos_bitmap(resource);
    if (result.bitmap.flags != 0x07 || result.bitmap.codec != 2
        || result.bitmap.width != 318 || result.bitmap.height != 197
        || result.bitmap.max_palette_index != 15) {
        throw std::runtime_error("Unsupported Millennium DOS LAST.LIB bitmap profile");
    }
    result.palette = decode_millennium_dos_palette(resource, result.bitmap);
    result.rgba = colorize_millennium_dos_bitmap(result.bitmap, result.palette);
    return result;
}

} // namespace eon
