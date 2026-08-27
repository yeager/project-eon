#include "data/millennium_dos_last_screen.hpp"

#include "data/millennium_dos_lib.hpp"

#include <stdexcept>

namespace eon {

MillenniumDosLastScreen parse_millennium_dos_last_screen(
    const std::span<const std::uint8_t> last_lib) {
    const MillenniumDosLib library({last_lib.begin(), last_lib.end()});
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
