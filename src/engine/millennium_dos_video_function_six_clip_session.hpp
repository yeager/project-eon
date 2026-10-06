#pragma once

#include "data/millennium_dos_video_driver.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace eon {

enum class MillenniumDosVideoFunctionSixClipState {
    awaiting_width,
    awaiting_clip_limit,
    awaiting_mcga_unsigned_width,
    awaiting_mcga_signed_width,
    prefix_boundary,
    ret_boundary,
};

struct MillenniumDosVideoFunctionSixWordRead {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    std::uint16_t es = 0;
    std::uint16_t offset = 0;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixWordRead&) const = default;
};

struct MillenniumDosVideoFunctionSixWordWrite {
    std::uint16_t instruction_address = 0;
    std::uint16_t es = 0;
    std::uint16_t offset = 0;
    std::uint16_t previous_value = 0;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixWordWrite&) const = default;
};

enum class MillenniumDosVideoFunctionSixClipEndpoint {
    // The original function has selected its width-nonpositive RET opcode;
    // the session does not pop a caller return address.
    ret,
    // Stop immediately before the first instruction following the clip prefix.
    caller_driven_prefix,
};

struct MillenniumDosVideoFunctionSixClipOutcome {
    MillenniumDosVideoFunctionSixClipEndpoint endpoint{};
    std::uint16_t instruction_address = 0;
    std::optional<std::uint16_t> clipped_value;
    std::vector<MillenniumDosVideoFunctionSixWordRead> reads;
    std::optional<MillenniumDosVideoFunctionSixWordWrite> write;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixClipOutcome&) const = default;
};

struct MillenniumDosVideoFunctionSixClipBoundary {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    std::uint16_t es = 0;
    std::uint16_t offset = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixClipBoundary&) const = default;
};

// Hash-bound execution of only the entry-side clipping prefix in the English
// EGA640/MCGA private function $06. Descriptor words are explicit runtime
// observations. It stops before any source-pointer access, helper call, VGA
// operation, or pixel write. Verified caller setup uses descriptor +$08 as X
// and +$10 as width.
class MillenniumDosVideoFunctionSixClipSession {
public:
    MillenniumDosVideoFunctionSixClipSession(
        std::span<const std::uint8_t> english_driver,
        MillenniumDosVideoDriverKind kind,
        std::uint16_t descriptor_es,
        std::uint16_t descriptor_bx);

    [[nodiscard]] MillenniumDosVideoFunctionSixClipState state() const { return state_; }
    [[nodiscard]] std::optional<MillenniumDosVideoFunctionSixClipBoundary> boundary() const;
    [[nodiscard]] const std::optional<MillenniumDosVideoFunctionSixClipOutcome>& outcome() const {
        return outcome_;
    }

    void observe_word_read(const MillenniumDosVideoFunctionSixWordRead& read);

private:
    [[nodiscard]] std::uint16_t descriptor_field_offset(std::uint16_t field) const;
    void finish(MillenniumDosVideoFunctionSixClipEndpoint endpoint,
        std::uint16_t instruction_address, std::optional<std::uint16_t> clipped_value);

    MillenniumDosVideoDriverProfile driver_;
    MillenniumDosVideoDriverKind kind_;
    std::uint16_t descriptor_es_ = 0;
    std::uint16_t descriptor_bx_ = 0;
    MillenniumDosVideoFunctionSixClipState state_{};
    std::uint64_t last_sequence_ = 0;
    std::optional<MillenniumDosVideoFunctionSixWordRead> width_read_;
    std::uint16_t clipped_value_ = 0;
    std::vector<MillenniumDosVideoFunctionSixWordRead> reads_;
    std::optional<MillenniumDosVideoFunctionSixWordWrite> write_;
    std::optional<MillenniumDosVideoFunctionSixClipOutcome> outcome_;
};

} // namespace eon
