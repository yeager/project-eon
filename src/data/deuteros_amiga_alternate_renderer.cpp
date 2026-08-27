#include "data/deuteros_amiga_alternate_renderer.hpp"

#include <array>
#include <stdexcept>

namespace eon {
namespace {

std::uint8_t byte_at(const DeuterosAmigaMainResourceTransfer& transfer, std::size_t offset) {
    if (offset >= transfer.payload.size()) {
        throw std::runtime_error("Deuteros alternate renderer stream exceeds original payload");
    }
    return transfer.payload[offset];
}

std::uint16_t big16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw std::runtime_error("Truncated Deuteros alternate renderer word");
    }
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8U)
        | bytes[offset + 1]);
}

std::uint32_t big32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(big16(bytes, offset)) << 16U) | big16(bytes, offset + 2);
}

std::size_t main_offset(const DeuterosAmigaLoadPlan& plan, std::uint32_t address,
    std::size_t length) {
    if (address < plan.main_stage.destination
        || address - plan.main_stage.destination > plan.main_stage.length
        || length > plan.main_stage.length - (address - plan.main_stage.destination)) {
        throw std::runtime_error("Deuteros alternate renderer address outside original main stage");
    }
    return static_cast<std::size_t>(address - plan.main_stage.destination);
}

} // namespace

DeuterosAmigaAlternateRendererTrace trace_deuteros_amiga_alternate_renderer(
    const DeuterosAmigaMainResourceTransfer& transfer,
    const DeuterosAmigaMainStageEntry& entry, std::uint32_t stream_address) {
    if (transfer.payload_destination_address != entry.resource_payload_address
        || transfer.payload.size() != transfer.payload_length || transfer.payload.size() < 4) {
        throw std::runtime_error("Invalid Deuteros alternate renderer payload ownership");
    }
    if (stream_address < transfer.payload_destination_address) {
        throw std::runtime_error("Deuteros alternate renderer stream precedes original payload");
    }
    const auto offset = static_cast<std::uint64_t>(stream_address)
        - transfer.payload_destination_address;
    if (offset >= transfer.payload.size()) {
        throw std::runtime_error("Deuteros alternate renderer stream outside original payload");
    }

    DeuterosAmigaAlternateRendererTrace trace;
    trace.stream_address = stream_address;
    trace.stream_offset = static_cast<std::uint32_t>(offset);
    std::size_t cursor = static_cast<std::size_t>(offset);
    bool positioned = false;
    bool primary_selected = false;
    bool secondary_selected = false;
    constexpr std::uint32_t bytes_per_text_row = 0x28;

    // This loop is intentionally not a generalized $20580 emulator. These
    // are the opcode classes proved by the real $32a24+$0b38 opening stream.
    while (true) {
        const auto command = byte_at(transfer, cursor++);
        if (command == 0) {
            if (!positioned || !primary_selected || !secondary_selected) {
                throw std::runtime_error("Incomplete Deuteros alternate renderer opening stream");
            }
            return trace;
        }
        if (command == 0x16) {
            trace.position_column = byte_at(transfer, cursor++);
            trace.position_row = byte_at(transfer, cursor++);
            const auto capped_row = static_cast<std::uint32_t>(
                trace.position_row >= 0x31 ? 0x30 : trace.position_row);
            // $2069c: column + ((min(row,$30) << 2) * $28), added to $20128.
            trace.primary_video_offset = static_cast<std::uint32_t>(trace.position_column)
                + (capped_row << 2U) * bytes_per_text_row;
            positioned = true;
            continue;
        }
        if (command == 0x10) {
            trace.primary_table_selector = byte_at(transfer, cursor++) & 0x0fU;
            primary_selected = true;
            continue;
        }
        if (command == 0x11) {
            trace.secondary_table_selector = byte_at(transfer, cursor++) & 0x0fU;
            secondary_selected = true;
            continue;
        }
        // $20650 sends bit-7-clear codes through $206e6. Its real writes use
        // global video and font pointers whose setup is not yet recovered, so
        // preserve the exact glyph request but do not invent pixels.
        if (command >= 0x20 && (command & 0x80U) == 0) {
            trace.glyph_codes.push_back(command);
            continue;
        }
        throw std::runtime_error("Unsupported Deuteros alternate renderer command in original stream");
    }
}

void apply_deuteros_amiga_alternate_renderer(
    DeuterosAmigaFrame& frame, const AmigaAdf& disk,
    const DeuterosAmigaLoadPlan& plan,
    const DeuterosAmigaAlternateRendererTrace& trace) {
    constexpr std::uint32_t font_pointer_cell = 0x20538;
    constexpr std::uint32_t glyph_increment_cell = 0x2053c;
    constexpr std::uint32_t selector_table_address = 0x20488;
    constexpr std::uint32_t expected_font_address = 0x201b0;
    constexpr std::size_t bytes_per_row = DeuterosAmigaFrame::width / 8;
    constexpr std::size_t bytes_per_plane = bytes_per_row * DeuterosAmigaFrame::height;

    if (frame.color_indices.size()
        != static_cast<std::size_t>(DeuterosAmigaFrame::width) * DeuterosAmigaFrame::height) {
        throw std::runtime_error("Invalid Deuteros alternate renderer frame dimensions");
    }
    const auto main = disk.bytes(plan.main_stage.disk_offset, plan.main_stage.length);
    // $206e6 reads these globals after the main entry's $20068/$2013a setup.
    // Their initial values live in the genuine raw main stage and give the
    // embedded 8-byte glyph rows their real source and advance amount.
    const auto font_cell = main_offset(plan, font_pointer_cell, 4);
    const auto increment_cell = main_offset(plan, glyph_increment_cell, 4);
    if (big32(main, font_cell) != expected_font_address || big32(main, increment_cell) != 1) {
        throw std::runtime_error("Unsupported Deuteros alternate font global layout");
    }
    if (trace.primary_table_selector > 0x0f || trace.secondary_table_selector > 0x0f
        || trace.primary_video_offset >= bytes_per_plane) {
        throw std::runtime_error("Invalid Deuteros alternate renderer trace layout");
    }
    const auto table_offset = [&](std::uint8_t selector) {
        return main_offset(plan, selector_table_address + static_cast<std::uint32_t>(selector) * 8, 8);
    };
    const auto primary_table = main.subspan(table_offset(trace.primary_table_selector), 8);
    const auto secondary_table = main.subspan(table_offset(trace.secondary_table_selector), 8);
    std::array<std::uint8_t, 4> primary_masks{};
    std::array<std::uint8_t, 4> secondary_masks{};
    for (std::size_t plane = 0; plane < 4; ++plane) {
        primary_masks[plane] = static_cast<std::uint8_t>(big16(primary_table, plane * 2));
        secondary_masks[plane] = static_cast<std::uint8_t>(big16(secondary_table, plane * 2));
    }

    std::size_t video_offset = trace.primary_video_offset;
    for (const auto glyph : trace.glyph_codes) {
        if (glyph < 0x20) throw std::runtime_error("Invalid Deuteros alternate glyph code");
        const auto glyph_address = expected_font_address
            + static_cast<std::uint32_t>(glyph - 0x20U) * 8U;
        const auto glyph_rows = main.subspan(main_offset(plan, glyph_address, 8), 8);
        const auto x_byte = video_offset % bytes_per_row;
        const auto y = video_offset / bytes_per_row;
        if (x_byte >= bytes_per_row || y + glyph_rows.size() > DeuterosAmigaFrame::height) {
            throw std::runtime_error("Deuteros alternate glyph write outside original display plane");
        }
        for (std::size_t row = 0; row < glyph_rows.size(); ++row) {
            const auto glyph_bits = glyph_rows[row];
            const auto pixel_base = (y + row) * DeuterosAmigaFrame::width + x_byte * 8;
            for (std::size_t plane = 0; plane < 4; ++plane) {
                const auto output = static_cast<std::uint8_t>(
                    (static_cast<std::uint8_t>(~glyph_bits) & secondary_masks[plane])
                    | (glyph_bits & primary_masks[plane]));
                for (std::size_t bit = 0; bit < 8; ++bit) {
                    const auto old_index = frame.color_indices[pixel_base + bit];
                    const auto plane_bit = static_cast<std::uint8_t>((output >> (7U - bit)) & 1U);
                    frame.color_indices[pixel_base + bit] = static_cast<std::uint8_t>(
                        (old_index & static_cast<std::uint8_t>(~(1U << plane)))
                        | (plane_bit << plane));
                }
            }
        }
        ++video_offset; // $2053c is verified above as the original longword one.
    }
}

} // namespace eon
