#pragma once

#include "data/millennium_dos_game_flow.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace eon {

enum class MillenniumDosSeventhFunctionBoundaryKind {
    runtime_word,
    runtime_byte,
    call_return,
    returned_register_bx,
    local_return,
};

struct MillenniumDosSeventhFunctionBoundary {
    MillenniumDosSeventhFunctionBoundaryKind kind =
        MillenniumDosSeventhFunctionBoundaryKind::runtime_word;
    std::uint16_t instruction_address = 0;
    std::optional<std::uint16_t> runtime_address;
    std::optional<std::uint16_t> call_target;
    std::optional<std::uint16_t> known_ax;
    std::optional<std::uint8_t> known_al;
    std::optional<std::uint16_t> known_bx;
    std::size_t ordinal = 0;
    constexpr bool operator==(const MillenniumDosSeventhFunctionBoundary&) const = default;
};

struct MillenniumDosSeventhFunctionWordEffect {
    std::uint16_t address = 0;
    std::uint16_t previous = 0;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosSeventhFunctionWordEffect&) const = default;
};

// Manual recompilation of the exact English $7521..$7596 handler. It is an
// isolated recovery entry: neither the raw action nor dispatcher reachability
// is inferred, and every native read/call return remains an observation.
class MillenniumDosSeventhFunctionSession {
public:
    explicit MillenniumDosSeventhFunctionSession(
        std::span<const std::uint8_t> game_executable);

    [[nodiscard]] MillenniumDosSeventhFunctionBoundary boundary() const;
    [[nodiscard]] bool returned() const { return step_ == terminal_step; }
    [[nodiscard]] bool returned_by_guard() const { return returned_by_guard_; }
    [[nodiscard]] const std::vector<MillenniumDosSeventhFunctionWordEffect>&
    runtime_effects() const { return runtime_effects_; }

    void observe_runtime_word(std::uint16_t instruction_address,
        std::uint16_t runtime_address, std::uint16_t value);
    void observe_runtime_byte(std::uint16_t instruction_address,
        std::uint16_t runtime_address, std::uint8_t value);
    void observe_call_return(std::uint16_t call_address,
        std::uint16_t return_address);
    void observe_returned_bx(std::uint16_t store_address, std::uint16_t value);

private:
    static constexpr std::size_t terminal_step = 27;
    void require_boundary(MillenniumDosSeventhFunctionBoundaryKind kind,
        std::uint16_t instruction_address,
        std::optional<std::uint16_t> runtime_address = std::nullopt) const;

    std::span<const std::uint8_t> game_executable_;
    MillenniumDosSeventhFunctionKeyTrace trace_;
    std::size_t step_ = 0;
    std::size_t call_ordinal_ = 0;
    bool returned_by_guard_ = false;
    std::optional<std::uint8_t> da17_;
    std::optional<std::uint16_t> saved_05ca_;
    std::optional<std::uint16_t> da18_;
    std::optional<std::uint16_t> da27_;
    std::optional<std::uint8_t> da26_;
    std::optional<std::uint16_t> da35_;
    std::optional<std::uint16_t> da37_;
    std::vector<MillenniumDosSeventhFunctionWordEffect> runtime_effects_;
};

} // namespace eon
