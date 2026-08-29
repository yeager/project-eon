#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace eon {

// A literal retained from original media. `record_offset` and `record_size`
// identify the surrounding raw record when one is known; `literal_offset`
// identifies the printable substring without interpreting its native control
// bytes as a UI format or an input binding.
struct MillenniumControlTextLiteralEvidence {
    std::uint32_t record_offset = 0;
    std::uint32_t record_size = 0;
    std::uint32_t literal_offset = 0;
    std::string literal;
    std::string record_sha256;
};

// The English and Spanish DOS 2200AD4.BIN pointer tables directly select
// these raw records. Pointer indices are source-media facts, not command
// identifiers or reconstructed controls. No executable caller from the loaded
// buffer to an input dispatcher has been recovered.
struct MillenniumDosControlTextEvidence {
    std::string source_sha256;
    std::array<std::size_t, 5> pointer_indices{};
    std::array<MillenniumControlTextLiteralEvidence, 5> literals{};
};

// The supplied Millennium Atari ST physical-dump leaf contains this exact
// raw span. It is deliberately not passed through an STX filesystem or track
// decoder: the bytes establish text provenance only, not a runnable Atari
// input route.
struct MillenniumAtariPhysicalControlTextEvidence {
    std::string source_sha256;
    std::uint32_t span_offset = 0;
    std::uint32_t span_size = 0;
    std::string span_sha256;
    std::array<MillenniumControlTextLiteralEvidence, 5> literals{};
};

[[nodiscard]] MillenniumDosControlTextEvidence parse_millennium_dos_control_text_evidence(
    std::span<const std::uint8_t> static_data);

[[nodiscard]] MillenniumAtariPhysicalControlTextEvidence
parse_millennium_atari_physical_control_text_evidence(
    std::span<const std::uint8_t> physical_dump);

} // namespace eon
