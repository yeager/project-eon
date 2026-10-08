#pragma once

#include "data/millennium_dos_text_driver.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace eon {

struct MillenniumDosInt93InstallationObservation {
    std::string loaded_driver_sha256;
    std::uint16_t loaded_driver_segment = 0;
    std::uint16_t vector_segment = 0;
    std::uint16_t vector_offset = 0;
};

struct MillenniumDosInt93CallObservation {
    std::uint64_t sequence = 0;
    std::uint16_t handler_segment = 0;
    std::uint16_t handler_offset = 0;
    std::uint8_t ah = 0;
    std::uint8_t al = 0;
    std::uint16_t dx = 0;
    // AH=7 reads CS:[0020]; AH=8/9 read CS:[0022].
    std::uint16_t current_cursor_word = 0;
    std::uint16_t saved_page_word = 0;
};

struct MillenniumDosInt93TextWrite {
    std::uint16_t segment = 0;
    std::uint16_t offset = 0;
    std::uint8_t width_bytes = 0;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosInt93TextWrite&) const = default;
};

struct MillenniumDosInt93TextOutcome {
    std::uint64_t sequence = 0;
    std::uint16_t handler_offset = 0;
    // The handler's near RET reaches the dispatcher's cleanup at this offset.
    std::uint16_t dispatcher_cleanup_offset = 0x001b;
    std::vector<MillenniumDosInt93TextWrite> writes;
};

// Standalone execution model for hash-identified AH=0/3/4/5/6/7/8/9 handler bodies.
// The constructor requires an observed loaded identity and INT 93h vector;
// it does not infer that the game reached or installed either one.
class MillenniumDosInt93TextSession {
public:
    MillenniumDosInt93TextSession(
        std::span<const std::uint8_t> driver,
        MillenniumDosTextDriverKind kind,
        MillenniumDosInt93InstallationObservation installation);

    [[nodiscard]] const MillenniumDosTextDriverProfile& profile() const { return profile_; }
    [[nodiscard]] MillenniumDosInt93TextOutcome observe_call(
        const MillenniumDosInt93CallObservation& observation);

private:
    MillenniumDosTextDriverProfile profile_;
    std::uint16_t driver_segment_ = 0;
    std::uint64_t last_sequence_ = 0;
};

} // namespace eon
