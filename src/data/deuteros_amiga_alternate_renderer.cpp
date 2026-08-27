#include "data/deuteros_amiga_alternate_renderer.hpp"

#include <stdexcept>

namespace eon {
namespace {

std::uint8_t byte_at(const DeuterosAmigaMainResourceTransfer& transfer, std::size_t offset) {
    if (offset >= transfer.payload.size()) {
        throw std::runtime_error("Deuteros alternate renderer stream exceeds original payload");
    }
    return transfer.payload[offset];
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

} // namespace eon
