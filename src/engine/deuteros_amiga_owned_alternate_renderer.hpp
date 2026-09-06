#pragma once

#include "engine/native_runtime_memory.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace eon {

// Execute the recovered $20580 byte-stream classes against owned native
// memory. Read/write are supplied by the caller so a complete frame remains
// private until its transaction commits. This is the original four-plane
// operation; it neither rasterises a host font nor reads game media again.
template<class Read, class Write>
void apply_deuteros_amiga_owned_alternate_renderer(
    const std::uint32_t stream_address, Read&& read, Write&& write) {
    using Width = MemoryTransferElementWidth;
    constexpr std::uint32_t video_base_cell = 0x20128;
    constexpr std::uint32_t video_cursor_cell = 0x20510;
    constexpr std::uint32_t primary_table_cell = 0x20508;
    constexpr std::uint32_t secondary_table_cell = 0x2050c;
    constexpr std::uint32_t selector_table = 0x20488;
    constexpr std::uint32_t font_pointer_cell = 0x20538;
    constexpr std::uint32_t glyph_increment_cell = 0x2053c;
    constexpr std::uint32_t plane_bytes = 8000;
    constexpr std::uint32_t row_bytes = 40;
    constexpr std::size_t command_budget = 4096;

    if (stream_address == 0 || stream_address >= 0x1000000U)
        throw std::runtime_error("Deuteros alternate stream is outside native memory");
    const auto font = read(font_pointer_cell, 4);
    const auto increment = read(glyph_increment_cell, 4);
    if (font != 0x201b0 || increment != 1)
        throw std::runtime_error("Deuteros alternate renderer globals are unsupported");

    std::uint32_t cursor = stream_address;
    bool positioned = false, primary_selected = false, secondary_selected = false;
    for (std::size_t commands = 0; commands < command_budget; ++commands) {
        const auto opcode = static_cast<std::uint8_t>(read(cursor++, 1));
        if (opcode == 0) {
            if (!positioned || !primary_selected || !secondary_selected)
                throw std::runtime_error("Deuteros alternate renderer stream is incomplete");
            return;
        }
        if (opcode == 0x16) {
            const auto column = static_cast<std::uint8_t>(read(cursor++, 1));
            const auto row = static_cast<std::uint8_t>(read(cursor++, 1));
            const auto capped_row = static_cast<std::uint32_t>(row >= 0x31 ? 0x30 : row);
            const auto offset = static_cast<std::uint32_t>(column) + (capped_row << 2U) * row_bytes;
            if (offset >= plane_bytes)
                throw std::runtime_error("Deuteros alternate renderer position exceeds a plane");
            write(video_cursor_cell, Width::longword, read(video_base_cell, 4) + offset);
            positioned = true;
            continue;
        }
        if (opcode == 0x10 || opcode == 0x11) {
            const auto selector = static_cast<std::uint8_t>(read(cursor++, 1)) & 0x0fU;
            write(opcode == 0x10 ? primary_table_cell : secondary_table_cell,
                Width::longword, selector_table + static_cast<std::uint32_t>(selector) * 8U);
            if (opcode == 0x10) primary_selected = true;
            else secondary_selected = true;
            continue;
        }
        if (opcode < 0x20 || (opcode & 0x80U) != 0)
            throw std::runtime_error("Unsupported Deuteros alternate renderer command");
        if (!positioned || !primary_selected || !secondary_selected)
            throw std::runtime_error("Deuteros alternate glyph precedes renderer setup");

        const auto destination = read(video_cursor_cell, 4);
        const auto video_base = read(video_base_cell, 4);
        if (destination < video_base || destination - video_base >= plane_bytes
            || (destination - video_base) / row_bytes + 8U > 200U)
            throw std::runtime_error("Deuteros alternate glyph exceeds the original display plane");
        const auto primary = read(primary_table_cell, 4);
        const auto secondary = read(secondary_table_cell, 4);
        const auto glyph = font + static_cast<std::uint32_t>(opcode - 0x20U) * 8U;
        for (std::uint32_t plane = 0; plane < 4; ++plane) {
            const auto primary_mask = static_cast<std::uint8_t>(read(primary + plane * 2U, 2));
            const auto secondary_mask = static_cast<std::uint8_t>(read(secondary + plane * 2U, 2));
            for (std::uint32_t row = 0; row < 8; ++row) {
                const auto bits = static_cast<std::uint8_t>(read(glyph + row, 1));
                const auto output = static_cast<std::uint8_t>((static_cast<std::uint8_t>(~bits)
                    & secondary_mask) | (bits & primary_mask));
                const auto address = destination + plane * plane_bytes + row * row_bytes;
                if (address >= 0x1000000U)
                    throw std::runtime_error("Deuteros alternate renderer write exceeds native memory");
                write(address, Width::byte, output);
            }
        }
        const auto next = destination + increment;
        if (next >= 0x1000000U)
            throw std::runtime_error("Deuteros alternate renderer cursor exceeds native memory");
        write(video_cursor_cell, Width::longword, next);
    }
    throw std::runtime_error("Deuteros alternate renderer command budget exhausted");
}

} // namespace eon
