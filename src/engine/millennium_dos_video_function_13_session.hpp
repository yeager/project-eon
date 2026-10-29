#pragma once

#include "data/millennium_dos_video_driver.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace eon {

enum class MillenniumDosVideoFunction13State {
    awaiting_retrace_clear,
    awaiting_retrace_set,
    ret_boundary,
};

struct MillenniumDosVideoFunction13Boundary {
    std::uint16_t instruction_address = 0;
    std::uint16_t port_address = 0;
    constexpr bool operator==(const MillenniumDosVideoFunction13Boundary&) const = default;
};

struct MillenniumDosVideoFunction13PortRead {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    std::uint16_t port_address = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunction13PortRead&) const = default;
};

struct MillenniumDosVideoFunction13Outcome {
    std::uint16_t ret_instruction_address = 0;
    std::vector<MillenniumDosVideoFunction13PortRead> reads;
};

// Standalone, hash-bound poll loop for English EGA640/MCGA function $13.
// Every VGA status byte is explicit input; the session does not read hardware,
// execute RET, select a driver, or assert driver installation/reachability.
class MillenniumDosVideoFunction13Session {
public:
    MillenniumDosVideoFunction13Session(
        std::span<const std::uint8_t> english_driver,
        MillenniumDosVideoDriverKind kind);

    [[nodiscard]] MillenniumDosVideoFunction13State state() const { return state_; }
    [[nodiscard]] std::uint64_t next_sequence() const { return last_sequence_ + 1; }
    [[nodiscard]] const std::vector<MillenniumDosVideoFunction13PortRead>& reads() const {
        return reads_;
    }
    [[nodiscard]] std::optional<MillenniumDosVideoFunction13Boundary> boundary() const;
    [[nodiscard]] std::optional<MillenniumDosVideoFunction13Outcome> outcome() const {
        return outcome_;
    }

    void observe_status(const MillenniumDosVideoFunction13PortRead& read);

private:
    MillenniumDosVideoDriverProfile driver_;
    MillenniumDosVideoFunction13State state_ =
        MillenniumDosVideoFunction13State::awaiting_retrace_clear;
    std::uint64_t last_sequence_ = 0;
    std::vector<MillenniumDosVideoFunction13PortRead> reads_;
    std::optional<MillenniumDosVideoFunction13Outcome> outcome_;
};

} // namespace eon
