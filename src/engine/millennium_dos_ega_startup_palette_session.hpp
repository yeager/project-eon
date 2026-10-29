#pragma once

#include "data/millennium_dos_game_flow.hpp"
#include "engine/millennium_dos_game_function_four_dispatch.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace eon {

struct MillenniumDosEgaPaletteWriteObservation {
    std::uint64_t sequence = 0;
    std::uint16_t interrupt_instruction_address = 0;
    std::uint8_t interrupt_number = 0;
    std::uint16_t ax = 0;
    std::uint16_t bx = 0;
};

struct MillenniumDosEgaPaletteWriteResult {
    bool accepted = false;
    std::string error;
    MillenniumDosEgaPaletteRegisterWrite write{};
    std::size_t ordinal = 0;
    std::uint16_t next_instruction_address = 0;
    bool complete = false;
};

// Tracks the hash-identified EGA startup write table. The return-only API
// validates an ordered BIOS request sequence; the applied-write API is for a
// native display adapter and must not be used to infer hardware effects from
// a call return. Both stop at the loop RET ($047c).
class MillenniumDosEgaStartupPaletteSession {
public:
    MillenniumDosEgaStartupPaletteSession(
        std::span<const std::uint8_t> exact_game_executable,
        const MillenniumDosGameFunctionFourDispatchResult& function_four);

    [[nodiscard]] std::optional<MillenniumDosEgaPaletteRegisterWrite>
    pending_write() const;
    [[nodiscard]] std::uint16_t next_instruction_address() const;
    [[nodiscard]] std::size_t completed_write_count() const noexcept { return next_ordinal_; }
    [[nodiscard]] bool complete() const noexcept { return next_ordinal_ == writes_.size(); }
    [[nodiscard]] const std::array<std::optional<std::uint8_t>, 16>&
    palette_registers() const noexcept { return palette_registers_; }

    [[nodiscard]] MillenniumDosEgaPaletteWriteResult observe_applied_write(
        const MillenniumDosEgaPaletteWriteObservation& observation);
    [[nodiscard]] MillenniumDosEgaPaletteWriteResult observe_write_return(
        const MillenniumDosEgaPaletteWriteObservation& observation);

private:
    std::array<MillenniumDosEgaPaletteRegisterWrite, 16> writes_{};
    std::array<std::optional<std::uint8_t>, 16> palette_registers_{};
    std::size_t next_ordinal_ = 0;
    std::uint64_t last_sequence_ = 0;
};

} // namespace eon
