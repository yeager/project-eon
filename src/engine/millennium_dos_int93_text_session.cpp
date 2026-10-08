#include "engine/millennium_dos_int93_text_session.hpp"

#include <stdexcept>

namespace eon {

MillenniumDosInt93TextSession::MillenniumDosInt93TextSession(
    const std::span<const std::uint8_t> driver, const MillenniumDosTextDriverKind kind,
    MillenniumDosInt93InstallationObservation installation)
    : profile_(parse_millennium_dos_text_driver(driver, kind)),
      driver_segment_(installation.loaded_driver_segment) {
    if (installation.loaded_driver_sha256 != profile_.sha256
        || installation.vector_segment != driver_segment_
        || installation.vector_offset != 0) {
        throw std::runtime_error("Millennium DOS INT 93h installation observation mismatch");
    }
}

MillenniumDosInt93TextOutcome MillenniumDosInt93TextSession::observe_call(
    const MillenniumDosInt93CallObservation& observation) {
    if (observation.sequence == 0 || observation.sequence <= last_sequence_
        || observation.ah >= profile_.handler_offsets.size()
        || observation.handler_segment != driver_segment_
        || observation.handler_offset != profile_.handler_offsets[observation.ah]) {
        throw std::runtime_error("Millennium DOS INT 93h handler observation mismatch");
    }

    MillenniumDosInt93TextOutcome outcome{
        .sequence = observation.sequence,
        .handler_offset = observation.handler_offset};
    const auto word_write = [&](const std::uint16_t offset, const std::uint16_t value) {
        outcome.writes.push_back({driver_segment_, offset, 2, value});
    };
    const auto byte_write = [&](const std::uint16_t offset, const std::uint8_t value) {
        outcome.writes.push_back({driver_segment_, offset, 1, value});
    };
    switch (observation.ah) {
    case 0:
    case 6:
        // Both hash-bound table entries target the shared RET at $0026.
        break;
    case 3:
        word_write(0x0020, observation.dx);
        break;
    case 4:
        byte_write(0x0024, observation.al);
        if (profile_.kind == MillenniumDosTextDriverKind::ega6) {
            word_write(0x0688, static_cast<std::uint16_t>(0x0608U
                + 8U * static_cast<std::uint16_t>(observation.al)));
        }
        break;
    case 5:
        byte_write(0x0025, observation.al);
        if (profile_.kind == MillenniumDosTextDriverKind::ega6) {
            word_write(0x068a, static_cast<std::uint16_t>(0x0608U
                + 8U * static_cast<std::uint16_t>(observation.al)));
        }
        break;
    case 7:
        word_write(0x0022, observation.current_cursor_word);
        break;
    case 8: {
        const auto next_page = static_cast<std::uint16_t>(
            observation.saved_page_word + profile_.page_stride);
        word_write(0x0022, next_page);
        word_write(0x0020, next_page);
        break;
    }
    case 9:
        word_write(0x0020, observation.saved_page_word);
        break;
    default:
        throw std::runtime_error("Unsupported Millennium DOS INT 93h text operation");
    }
    last_sequence_ = observation.sequence;
    return outcome;
}

} // namespace eon
