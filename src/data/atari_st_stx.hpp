#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace eon {

// A read-only index over an STX physical floppy dump.  It deliberately does
// not produce a flat .st image: callers retain the supplied physical bytes and
// request identified sectors from their recorded locations.
struct AtariStStxSector {
    std::uint8_t track = 0;
    std::uint8_t side = 0;
    std::uint8_t id = 0;
    std::uint8_t size_code = 0;
    std::uint8_t fdc_status = 0;
    std::size_t payload_offset = 0;
    std::size_t payload_size = 0;
};

class AtariStStxPhysicalDisk {
public:
    explicit AtariStStxPhysicalDisk(std::vector<std::uint8_t> image);

    [[nodiscard]] std::size_t track_count() const { return track_count_; }
    [[nodiscard]] const std::vector<AtariStStxSector>& sectors() const { return sectors_; }
    [[nodiscard]] std::span<const std::uint8_t> sector(
        std::uint8_t track, std::uint8_t side, std::uint8_t id) const;

private:
    std::vector<std::uint8_t> image_;
    std::size_t track_count_ = 0;
    std::vector<AtariStStxSector> sectors_;
};

} // namespace eon
