#include "data/atari_st_prg.hpp"

#include <limits>
#include <stdexcept>

namespace eon {
namespace {

constexpr std::size_t header_bytes = 28;

std::uint16_t read_be16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw std::runtime_error("Truncated Atari ST PRG field");
    }
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8U)
        | bytes[offset + 1]);
}

std::uint32_t read_be32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw std::runtime_error("Truncated Atari ST PRG field");
    }
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U)
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 16U)
        | (static_cast<std::uint32_t>(bytes[offset + 2]) << 8U)
        | bytes[offset + 3];
}

} // namespace

AtariStPrg parse_atari_st_prg(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < header_bytes || read_be16(bytes, 0) != 0x601aU) {
        throw std::runtime_error("Unsupported Atari ST PRG header");
    }

    AtariStPrg result;
    result.text_bytes = read_be32(bytes, 2);
    result.data_bytes = read_be32(bytes, 6);
    result.bss_bytes = read_be32(bytes, 10);
    result.symbol_bytes = read_be32(bytes, 14);
    result.flags = read_be32(bytes, 22);
    result.absolute_flag = read_be16(bytes, 26);

    const auto payload = static_cast<std::uint64_t>(result.text_bytes) + result.data_bytes
        + result.symbol_bytes;
    if (payload > std::numeric_limits<std::size_t>::max()
        || header_bytes > bytes.size()
        || static_cast<std::size_t>(payload) > bytes.size() - header_bytes) {
        throw std::runtime_error("Atari ST PRG segments outside image");
    }
    std::size_t cursor = header_bytes + static_cast<std::size_t>(payload);
    if (result.absolute_flag != 0U) {
        if (cursor != bytes.size()) throw std::runtime_error("Unexpected Atari ST absolute PRG tail");
        return result;
    }
    if (bytes.size() - cursor < 4) throw std::runtime_error("Missing Atari ST PRG relocation table");
    auto relocation = read_be32(bytes, cursor);
    cursor += 4;
    if (relocation == 0U) {
        if (cursor != bytes.size()) throw std::runtime_error("Unexpected Atari ST empty relocation tail");
        return result;
    }
    const auto loadable_bytes = static_cast<std::uint64_t>(result.text_bytes) + result.data_bytes;
    if (loadable_bytes < 4U || relocation > loadable_bytes - 4U) {
        throw std::runtime_error("Atari ST PRG first relocation outside text/data");
    }
    result.first_relocation_offset = relocation;
    result.last_relocation_offset = relocation;
    result.relocation_count = 1;
    const auto loadable_offset = header_bytes;
    result.relocations.push_back({relocation, read_be32(bytes, loadable_offset + relocation)});
    while (cursor < bytes.size()) {
        const auto delta = bytes[cursor++];
        if (delta == 0U) {
            if (cursor != bytes.size()) throw std::runtime_error("Trailing Atari ST PRG relocation bytes");
            return result;
        }
        const auto increment = delta == 1U ? 254U : static_cast<std::uint32_t>(delta);
        if (relocation > std::numeric_limits<std::uint32_t>::max() - increment) {
            throw std::runtime_error("Atari ST PRG relocation overflow");
        }
        relocation += increment;
        if (relocation > loadable_bytes - 4U) {
            throw std::runtime_error("Atari ST PRG relocation outside text/data");
        }
        result.last_relocation_offset = relocation;
        ++result.relocation_count;
        result.relocations.push_back({relocation, read_be32(bytes, loadable_offset + relocation)});
    }
    throw std::runtime_error("Unterminated Atari ST PRG relocation table");
}

} // namespace eon
