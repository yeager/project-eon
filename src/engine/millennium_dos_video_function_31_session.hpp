#pragma once

#include "data/millennium_dos_video_driver.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace eon {

enum class MillenniumDosVideoFunction31State {
    awaiting_local_state_read,
    ret_boundary,
};

struct MillenniumDosVideoFunction31LocalRead {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    std::uint16_t ds = 0;
    std::uint16_t driver_local_address = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunction31LocalRead&) const = default;
};

struct MillenniumDosVideoFunction31Outcome {
    std::uint16_t ret_instruction_address = 0;
    MillenniumDosVideoFunction31LocalRead local_read;
    std::uint16_t ax = 0;
    constexpr bool operator==(const MillenniumDosVideoFunction31Outcome&) const = default;
};

// Standalone, hash-bound model of the English EGA640/MCGA private function
// $1f. The caller supplies the runtime DS-local byte explicitly; the session
// stops before RET and does not establish driver installation or call reachability.
class MillenniumDosVideoFunction31Session {
public:
    MillenniumDosVideoFunction31Session(
        std::span<const std::uint8_t> english_driver,
        MillenniumDosVideoDriverKind kind);

    [[nodiscard]] MillenniumDosVideoFunction31State state() const { return state_; }
    [[nodiscard]] std::optional<MillenniumDosVideoFunction31Outcome> outcome() const {
        return outcome_;
    }

    void observe_local_state_read(const MillenniumDosVideoFunction31LocalRead& read);

private:
    MillenniumDosVideoDriverProfile driver_;
    MillenniumDosVideoFunction31State state_ =
        MillenniumDosVideoFunction31State::awaiting_local_state_read;
    std::uint64_t last_sequence_ = 0;
    std::optional<MillenniumDosVideoFunction31Outcome> outcome_;
};

} // namespace eon
