#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace eon {

enum class MillenniumDosTextDriverKind { vga, ega6 };

// Hash-bound static identity of one supplied INT 93h text driver. This is
// separate from the INT 91h EGA640/MCGA video-driver profile.
struct MillenniumDosTextDriverProfile {
    MillenniumDosTextDriverKind kind{};
    std::size_t byte_size = 0;
    std::string sha256;
    std::array<std::uint16_t, 10> handler_offsets{};
    std::uint16_t page_stride = 0;
};

[[nodiscard]] MillenniumDosTextDriverProfile parse_millennium_dos_text_driver(
    std::span<const std::uint8_t> bytes, MillenniumDosTextDriverKind kind);

} // namespace eon
