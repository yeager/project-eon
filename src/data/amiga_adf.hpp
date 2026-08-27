#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace eon {

enum class AmigaDiskKind { dos, deuteros_data, unknown };

class AmigaAdf {
public:
    static constexpr std::size_t sector_size = 512;
    static constexpr unsigned cylinders = 80;
    static constexpr unsigned sides = 2;
    static constexpr unsigned sectors_per_track = 11;
    static constexpr std::size_t standard_size = sector_size * cylinders * sides * sectors_per_track;

    explicit AmigaAdf(std::vector<std::uint8_t> image);

    [[nodiscard]] AmigaDiskKind kind() const { return kind_; }
    [[nodiscard]] std::string identifier() const;
    [[nodiscard]] bool boot_checksum_valid() const;
    [[nodiscard]] std::uint32_t root_block() const;
    [[nodiscard]] std::span<const std::uint8_t, sector_size> sector(
        unsigned cylinder, unsigned side, unsigned sector) const;
    [[nodiscard]] std::span<const std::uint8_t, 1024> boot_block() const;

private:
    std::vector<std::uint8_t> image_;
    AmigaDiskKind kind_ = AmigaDiskKind::unknown;
};

} // namespace eon

