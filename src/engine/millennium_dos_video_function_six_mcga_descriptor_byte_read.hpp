#pragma once

#include <cstdint>

namespace eon {

struct MillenniumDosVideoFunctionSixMcgaDescriptorByteRead {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    std::uint16_t es = 0;
    std::uint16_t offset = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaDescriptorByteRead&) const = default;
};

} // namespace eon
