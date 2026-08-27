#include "data/millennium_dos_game_data.hpp"

#include <algorithm>
#include <stdexcept>

namespace eon {
namespace {

constexpr std::size_t celestial_label_count = 41;

} // namespace

MillenniumDosGameData parse_millennium_dos_game_data(
    std::span<const std::uint8_t> static_data) {
    // The first table string begins at 0x3d2 in the verified English binary.
    // Read every byte from the original media; hard-coded names would be a
    // synthetic substitute and would lose original padding/translation bytes.
    constexpr std::size_t table_offset = 0x3d2;
    if (table_offset >= static_data.size()) {
        throw std::runtime_error("Truncated Millennium DOS static data");
    }

    MillenniumDosGameData result;
    result.celestial_table_offset = table_offset;
    result.celestial_labels.reserve(celestial_label_count);
    std::size_t offset = table_offset;
    for (std::size_t index = 0; index < celestial_label_count; ++index) {
        if (offset >= static_data.size()) {
            throw std::runtime_error("Unsupported Millennium DOS celestial-label table");
        }
        const auto terminator = std::find(static_data.begin() + static_cast<std::ptrdiff_t>(offset),
            static_data.end(), std::uint8_t{0});
        if (terminator == static_data.end() || terminator == static_data.begin()
            + static_cast<std::ptrdiff_t>(offset)) {
            throw std::runtime_error("Unsupported Millennium DOS celestial-label table");
        }
        const auto length = static_cast<std::size_t>(terminator
            - (static_data.begin() + static_cast<std::ptrdiff_t>(offset)));
        const std::string value(reinterpret_cast<const char*>(static_data.data() + offset), length);
        result.celestial_labels.push_back({.source_offset = offset, .text = value});
        offset += length + 1;
    }
    return result;
}

} // namespace eon
