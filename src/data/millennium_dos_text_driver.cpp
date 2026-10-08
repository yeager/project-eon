#include "data/millennium_dos_text_driver.hpp"
#include "data/sha256.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>

namespace eon {
namespace {

std::uint16_t little16(const std::span<const std::uint8_t> bytes, const std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw std::runtime_error("Truncated Millennium DOS text-driver table");
    }
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset])
        | (static_cast<std::uint16_t>(bytes[offset + 1]) << 8U));
}

bool equals_at(const std::span<const std::uint8_t> bytes, const std::size_t offset,
    const std::span<const std::uint8_t> expected) {
    return offset <= bytes.size() && expected.size() <= bytes.size() - offset
        && std::equal(expected.begin(), expected.end(), bytes.begin()
            + static_cast<std::ptrdiff_t>(offset));
}

} // namespace

MillenniumDosTextDriverProfile parse_millennium_dos_text_driver(
    const std::span<const std::uint8_t> bytes, const MillenniumDosTextDriverKind kind) {
    constexpr std::array<std::uint8_t, 31> dispatcher{{
        0x1e,0x56,0x51,0x50,0x88,0xe0,0x32,0xe4,0xd1,0xe0,0x0e,0x1f,0xbe,0x27,0x00,
        0x01,0xc6,0xad,0x89,0xc1,0x58,0xbe,0x1b,0x00,0x56,0x51,0xc3,0x59,0x5e,0x1f,0xcf}};
    constexpr std::array<std::uint16_t, 10> vga_handlers{{
        0x0026,0x0330,0x038f,0x0040,0x03e9,0x03ee,0x0026,0x0046,0x03f3,0x004f}};
    constexpr std::array<std::uint16_t, 10> ega6_handlers{{
        0x0026,0x068c,0x0724,0x0040,0x07c3,0x07d7,0x0026,0x0046,0x07eb,0x004f}};
    constexpr std::string_view dispatcher_hash =
        "26ddd2baacb81d3f61d5c4950c5df444aa1edd4a0d93b02cbf8c82de111c8dad";
    const bool vga = kind == MillenniumDosTextDriverKind::vga;
    const std::size_t expected_size = vga ? 1024U : 2040U;
    const std::string_view expected_hash = vga
        ? "c31cb760d5f62a21b3baf9c09a6be413514780bd88eeac0273620e81b5d69318"
        : "063eefa58c98360d0ca2b4eaf9a77f8f9d13c619aee605dff6b0d0ee8b4a6b20";
    const std::string_view expected_table_hash = vga
        ? "0d9fc888e3d7e5f611d8857106a201448de0f95645ebb90607119a84246a9525"
        : "31d7a1e5fba25a172ce88ad032a2c7860f9220d5c2adcdfae0302ef73da5e102";
    const auto& handlers = vga ? vga_handlers : ega6_handlers;
    if (bytes.size() != expected_size || to_hex(sha256(bytes)) != expected_hash
        || to_hex(sha256(bytes.first(dispatcher.size()))) != dispatcher_hash
        || !equals_at(bytes, 0, dispatcher)) {
        throw std::runtime_error("Unsupported Millennium DOS INT 93h text driver");
    }
    if (to_hex(sha256(bytes.subspan(0x27, 20))) != expected_table_hash) {
        throw std::runtime_error("Unsupported Millennium DOS INT 93h text-driver table");
    }
    const auto has_span_hash = [&](const std::size_t offset, const std::size_t size,
        const std::string_view hash) {
        return offset <= bytes.size() && size <= bytes.size() - offset
            && to_hex(sha256(bytes.subspan(offset, size))) == hash;
    };
    if (!has_span_hash(0x40, 6,
            "298c53f64503a40ab15f68f7ee7cad72b912a394998cbaca08026ae21b9c59e6")
        || !has_span_hash(0x46, 9,
            "a0720b1221739c19c79ebbbf59b6d400aa04f468b834d5436e746d955c3ff1ae")
        || !has_span_hash(0x4f, 9,
            "9f87f593869ed8152ad1e34ebd38a3a57068ada368a92c0cb6052402f751d7c9")) {
        throw std::runtime_error("Unsupported Millennium DOS INT 93h cursor handlers");
    }
    if (vga) {
        if (!has_span_hash(0x3e9, 5,
                "fff2bdc122ec8ad7a50cdf881578dabc4cfdab87c6c18984c2cf2186b2ab5b41")
            || !has_span_hash(0x3ee, 5,
                "ae698ba77c3bdb5bab4fa9c58b791e7f6e81beb3c4ae3a7f607fa46f5fca09dc")
            || !has_span_hash(0x3f3, 13,
                "2851f4a262a8ee38ccb5e331f25a795fc0a172ce0a7d9fb7a5730523f4c427f3")) {
            throw std::runtime_error("Unsupported VGATXT INT 93h handler spans");
        }
    } else if (!has_span_hash(0x7c3, 20,
            "d1a1dda1d5d411531bdc4c8db6a35442df18d24c1c8e1cfaba2f9a510d6b7ece")
        || !has_span_hash(0x7d7, 20,
            "25cbca908c1c04b3db133079a6d26588493b78211efaf7b7cdc3985d4f3ec3f3")
        || !has_span_hash(0x7eb, 13,
            "002151afa70150d562f250674b15cf43432ada43f4487ec9e4f2379b26465a9c")) {
        throw std::runtime_error("Unsupported EG6TXT INT 93h handler spans");
    }
    for (std::size_t index = 0; index < handlers.size(); ++index) {
        if (little16(bytes, 0x27 + index * 2) != handlers[index]
            || handlers[index] >= bytes.size()) {
            throw std::runtime_error("Invalid Millennium DOS INT 93h text-driver target");
        }
    }
    if (bytes[0x26] != 0xc3) {
        throw std::runtime_error("Unsupported Millennium DOS INT 93h no-op handler");
    }
    return {kind, bytes.size(), std::string(expected_hash), handlers,
        static_cast<std::uint16_t>(vga ? 0x0a00 : 0x0280)};
}

} // namespace eon
