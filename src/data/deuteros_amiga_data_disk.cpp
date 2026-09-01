#include "data/deuteros_amiga_data_disk.hpp"

#include "data/sha256.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace eon {

DeuterosAmigaDataDiskHeader inspect_deuteros_amiga_data_disk_header(const AmigaAdf& disk) {
    constexpr std::size_t header_prefix_length = 0xc8;
    constexpr std::string_view marker = "DEUTEROSDATA";
    if (disk.kind() != AmigaDiskKind::deuteros_data || disk.identifier() != std::string("DEU\0", 4)
        || !disk.boot_checksum_valid() || disk.root_block() != 880) {
        throw std::runtime_error("Unsupported Deuteros Amiga custom data media");
    }
    const auto prefix = disk.bytes(0, header_prefix_length);
    std::size_t marker_count = 0;
    for (std::size_t offset = 0; offset + marker.size() <= prefix.size(); ++offset) {
        if (std::equal(marker.begin(), marker.end(), prefix.begin() + static_cast<std::ptrdiff_t>(offset))) {
            ++marker_count;
        }
    }
    if (marker_count == 0) {
        throw std::runtime_error("Deuteros Amiga custom data header lacks marker");
    }
    return {disk.identifier(), disk.root_block(), disk.boot_checksum_valid(),
        AmigaAdf::standard_size / AmigaAdf::sector_size, header_prefix_length,
        to_hex(sha256(prefix)), marker_count};
}

} // namespace eon
