#pragma once

#include "data/amiga_adf.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace eon {

// The supplied Deuteros data disk is custom DEU media, not an AmigaDOS
// volume. These header facts identify original media only; they do not assign
// a file system, directory, or resource semantics to its opaque payload.
struct DeuterosAmigaDataDiskHeader {
    std::string identifier;
    std::uint32_t root_block = 0;
    bool boot_checksum_valid = false;
    std::size_t sector_count = 0;
    std::size_t header_prefix_length = 0;
    std::string header_prefix_sha256;
    std::size_t data_marker_count = 0;
};

[[nodiscard]] DeuterosAmigaDataDiskHeader inspect_deuteros_amiga_data_disk_header(
    const AmigaAdf& disk);

} // namespace eon
