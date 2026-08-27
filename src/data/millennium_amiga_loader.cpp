#include "data/millennium_amiga_loader.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace eon {
namespace {

constexpr std::uint32_t bootstrap_disk_offset = 0x400;
constexpr std::uint32_t bootstrap_length = 0x400;
constexpr std::uint32_t bootstrap_destination = 0x70000;

std::uint32_t big32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw std::runtime_error("Truncated Millennium Amiga loader field");
    }
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U)
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 16U)
        | (static_cast<std::uint32_t>(bytes[offset + 2]) << 8U)
        | bytes[offset + 3];
}

void validate_range(const MillenniumAmigaLoadStage& stage) {
    if (stage.length == 0 || stage.disk_offset > AmigaAdf::standard_size
        || stage.length > AmigaAdf::standard_size - stage.disk_offset) {
        throw std::runtime_error("Millennium Amiga loader requests data outside ADF");
    }
}

} // namespace

MillenniumAmigaLoadPlan parse_millennium_amiga_load_plan(const AmigaAdf& disk) {
    if (disk.kind() != AmigaDiskKind::dos || !disk.boot_checksum_valid()) {
        throw std::runtime_error("Millennium Amiga loader requires a checksummed DOS ADF");
    }

    // These instructions are the recovered first raw read in the stage loaded
    // from disk offset $400. They establish the primary game stage at $41000.
    constexpr std::array<std::uint8_t, 14> first_prefix{{
        0x22, 0x3c, 0x00, 0x04, 0x10, 0x00, // move.l #$41000,d1
        0x20, 0x3c, 0x00, 0x02, 0x42, 0x00, // move.l #$24200,d0
        0x2e, 0x3c,                    // move.l #...,d7
    }};
    const auto loader = disk.bytes(bootstrap_disk_offset, bootstrap_length);
    const auto first = std::search(loader.begin(), loader.end(), first_prefix.begin(), first_prefix.end());
    if (first == loader.end()) throw std::runtime_error("Millennium Amiga first-stage read not found");
    const auto first_offset = static_cast<std::size_t>(first - loader.begin());
    if (first_offset + 34 > loader.size()) throw std::runtime_error("Truncated Millennium Amiga first-stage request");
    const auto first_chunk = big32(loader, first_offset + 14);
    const auto multiplier = static_cast<std::uint32_t>(loader[first_offset + 20]) << 8U
        | loader[first_offset + 21];
    if (loader[first_offset + 18] != 0xce || loader[first_offset + 19] != 0xfc
        || loader[first_offset + 22] != 0x4e || loader[first_offset + 23] != 0xb9
        || big32(loader, first_offset + 24) != 0x000661da) {
        throw std::runtime_error("Unexpected Millennium Amiga first-stage loader sequence");
    }
    if (first_chunk == 0 || multiplier == 0 || first_chunk > UINT32_MAX / multiplier) {
        throw std::runtime_error("Invalid Millennium Amiga first-stage length");
    }

    // The following request is issued after calling the first loaded stage.
    constexpr std::array<std::uint8_t, 14> resident_prefix{{
        0x22, 0x3c, 0x00, 0x06, 0x80, 0x00, // move.l #$68000,d1
        0x20, 0x3c, 0x00, 0x01, 0x64, 0x00, // move.l #$16400,d0
        0x2e, 0x3c,                    // move.l #...,d7
    }};
    const auto resident = std::search(loader.begin(), loader.end(), resident_prefix.begin(), resident_prefix.end());
    if (resident == loader.end()) throw std::runtime_error("Millennium Amiga resident-stage read not found");
    const auto resident_offset = static_cast<std::size_t>(resident - loader.begin());
    if (resident_offset + 50 > loader.size()) throw std::runtime_error("Truncated Millennium Amiga resident-stage request");
    const auto resident_chunk = big32(loader, resident_offset + 14);
    if (loader[resident_offset + 18] != 0xde || loader[resident_offset + 19] != 0x87
        || loader[resident_offset + 20] != 0x4e || loader[resident_offset + 21] != 0xb9
        || big32(loader, resident_offset + 22) != 0x000661da) {
        throw std::runtime_error("Unexpected Millennium Amiga resident-stage loader sequence");
    }
    // add.l d7,d7 doubles the $1600 I/O chunk before the resident request;
    // the original code then supplies the total length in d0 to the same loop.
    if (resident_chunk == 0 || resident_chunk > UINT32_MAX / 2U) {
        throw std::runtime_error("Invalid Millennium Amiga resident-stage length");
    }
    const auto resident_length = resident_chunk * 2U * 0x10U;
    const auto magic_offset = resident_offset + 54;
    if (big32(loader, magic_offset) != 0xa8d398fb) {
        throw std::runtime_error("Millennium Amiga loader handoff marker not found");
    }

    MillenniumAmigaLoadPlan plan{
        {bootstrap_disk_offset, bootstrap_length, bootstrap_destination},
        {0x24200, first_chunk * multiplier, 0x41000},
        {0x16400, resident_length, 0x68000},
        0x68000,
        big32(loader, magic_offset),
    };
    validate_range(plan.bootstrap_loader);
    validate_range(plan.first_stage);
    validate_range(plan.resident_stage);
    return plan;
}

MillenniumAmigaResidentEntry parse_millennium_amiga_resident_entry(
    const AmigaAdf& disk, const MillenniumAmigaLoadPlan& plan) {
    validate_range(plan.first_stage);
    validate_range(plan.resident_stage);
    if (plan.resident_entry != plan.resident_stage.destination) {
        throw std::runtime_error("Millennium Amiga resident entry is outside its loaded range");
    }

    constexpr std::array<std::uint8_t, 20> entry_prefix{{
        0x4e, 0xb9, 0x00, 0x07, 0x87, 0xd4, // jsr $787d4
        0x4a, 0x03,                         // tst.b d3
        0x67, 0x04,                         // beq.s past OR
        0x00, 0x40, 0x01, 0x00,             // ori.w #$0100,d0
        0x33, 0xc0, 0x00, 0x07, 0xb7, 0x5a, // move.w d0,$7b75a
    }};
    const auto bytes = disk.bytes(plan.resident_stage.disk_offset, entry_prefix.size() + 2U);
    if (!std::equal(entry_prefix.begin(), entry_prefix.end(), bytes.begin())
        || bytes[entry_prefix.size()] != 0x4e || bytes[entry_prefix.size() + 1U] != 0x75) {
        throw std::runtime_error("Unexpected Millennium Amiga resident entry gate");
    }

    constexpr std::uint32_t initializer_address = 0x787d4;
    const auto first_end = static_cast<std::uint64_t>(plan.first_stage.destination)
        + plan.first_stage.length;
    if (initializer_address < plan.first_stage.destination || initializer_address >= first_end) {
        throw std::runtime_error("Millennium Amiga resident initializer is outside first stage RAM");
    }
    return {plan.resident_entry, initializer_address, 0x7b75a, 0x0100};
}

} // namespace eon
