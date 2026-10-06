#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace eon {

struct MillenniumDosVideoFunctionSixMcgaOverlayRegisters {
    std::uint16_t ax = 0;
    std::uint16_t bx = 0;
    std::uint16_t cx = 0;
    std::uint16_t dx = 0;
    std::uint16_t si = 0;
    std::uint16_t di = 0;
    std::uint16_t bp = 0;
    std::uint16_t ds = 0;
    std::uint16_t es = 0;
    std::uint16_t ss = 0;
    std::uint16_t sp = 0;
    bool direction_flag = false;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaOverlayRegisters&) const = default;
};

struct MillenniumDosVideoFunctionSixMcgaOverlayStackWrite {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    std::uint16_t ss = 0;
    std::uint16_t offset = 0;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaOverlayStackWrite&) const = default;
};

struct MillenniumDosVideoFunctionSixMcgaOverlayStackRead {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    std::uint16_t ss = 0;
    std::uint16_t offset = 0;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaOverlayStackRead&) const = default;
};

struct MillenniumDosVideoFunctionSixMcgaOverlaySourceRead {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    std::uint16_t ds = 0;
    std::uint16_t offset = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaOverlaySourceRead&) const = default;
};

struct MillenniumDosVideoFunctionSixMcgaOverlayDestinationRead {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    std::uint16_t es = 0;
    std::uint16_t offset = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaOverlayDestinationRead&) const = default;
};

struct MillenniumDosVideoFunctionSixMcgaOverlayDestinationWrite {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    std::uint16_t es = 0;
    std::uint16_t offset = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaOverlayDestinationWrite&) const = default;
};

struct MillenniumDosVideoFunctionSixMcgaOverlayByteEffect {
    std::uint16_t source_offset = 0;
    std::uint16_t destination_offset = 0;
    std::uint8_t source_value = 0;
    std::uint8_t destination_before = 0;
    std::uint8_t destination_after = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaOverlayByteEffect&) const = default;
};

struct MillenniumDosVideoFunctionSixMcgaOverlayRow {
    std::uint16_t source_start = 0;
    std::uint16_t destination_start = 0;
    std::uint32_t byte_count = 0;
    std::uint16_t source_after_stride = 0;
    std::uint16_t destination_after_stride = 0;
    std::uint16_t popped_cx = 0;
    std::uint16_t bp_after = 0;
    std::vector<MillenniumDosVideoFunctionSixMcgaOverlayByteEffect> effects;
};

struct MillenniumDosVideoFunctionSixMcgaOverlayOutcome {
    MillenniumDosVideoFunctionSixMcgaOverlayRegisters initial_registers;
    MillenniumDosVideoFunctionSixMcgaOverlayRegisters final_registers;
    std::uint16_t instruction_boundary = 0x07c6;
    std::vector<MillenniumDosVideoFunctionSixMcgaOverlayRow> rows;
};

enum class MillenniumDosVideoFunctionSixMcgaOverlayState {
    awaiting_stack_push,
    awaiting_source_read,
    awaiting_destination_read,
    awaiting_destination_write,
    awaiting_stack_pop,
    complete,
};

// Hash-bound observation of MCGA.BIN [$07b5,$07c7). Memory bytes and stack
// reads are explicit inputs; this models only the listed 16-bit operations.
// It assigns no pixel meaning and establishes no runtime reachability.
class MillenniumDosVideoFunctionSixMcgaOverlayLoopSession {
public:
    MillenniumDosVideoFunctionSixMcgaOverlayLoopSession(
        std::span<const std::uint8_t> english_mcga_driver,
        MillenniumDosVideoFunctionSixMcgaOverlayRegisters initial_registers,
        std::uint32_t max_rows = 4096,
        std::uint32_t max_total_bytes = 1'048'576,
        std::uint64_t preceding_sequence = 0);

    [[nodiscard]] MillenniumDosVideoFunctionSixMcgaOverlayState state() const { return state_; }
    [[nodiscard]] const std::optional<MillenniumDosVideoFunctionSixMcgaOverlayOutcome>& outcome() const {
        return outcome_;
    }
    [[nodiscard]] const std::vector<MillenniumDosVideoFunctionSixMcgaOverlayStackWrite>& stack_writes() const {
        return stack_writes_;
    }
    [[nodiscard]] std::uint64_t last_sequence() const { return last_sequence_; }
    [[nodiscard]] const std::vector<MillenniumDosVideoFunctionSixMcgaOverlayByteEffect>& current_row_effects() const {
        return current_row_effects_;
    }

    void observe_stack_write(const MillenniumDosVideoFunctionSixMcgaOverlayStackWrite& write);
    void observe_source_read(const MillenniumDosVideoFunctionSixMcgaOverlaySourceRead& read);
    void observe_destination_read(const MillenniumDosVideoFunctionSixMcgaOverlayDestinationRead& read);
    void observe_destination_write(const MillenniumDosVideoFunctionSixMcgaOverlayDestinationWrite& write);
    void observe_stack_read(const MillenniumDosVideoFunctionSixMcgaOverlayStackRead& read);

private:
    void require_sequence(std::uint64_t sequence) const;
    void prepare_row();
    void finish_row();

    MillenniumDosVideoFunctionSixMcgaOverlayRegisters initial_registers_;
    MillenniumDosVideoFunctionSixMcgaOverlayRegisters registers_;
    std::uint32_t max_rows_ = 0;
    std::uint32_t max_total_bytes_ = 0;
    std::uint32_t rows_required_ = 0;
    std::uint32_t bytes_observed_ = 0;
    std::uint32_t row_bytes_remaining_ = 0;
    std::uint64_t last_sequence_ = 0;
    std::uint16_t pushed_cx_ = 0;
    std::uint16_t source_offset_ = 0;
    std::uint16_t destination_offset_ = 0;
    std::uint16_t current_row_source_start_ = 0;
    std::uint16_t current_row_destination_start_ = 0;
    std::uint8_t source_value_ = 0;
    std::uint8_t destination_before_ = 0;
    std::vector<MillenniumDosVideoFunctionSixMcgaOverlayStackWrite> stack_writes_;
    std::vector<MillenniumDosVideoFunctionSixMcgaOverlayByteEffect> current_row_effects_;
    std::vector<MillenniumDosVideoFunctionSixMcgaOverlayRow> rows_;
    std::optional<MillenniumDosVideoFunctionSixMcgaOverlayOutcome> outcome_;
    MillenniumDosVideoFunctionSixMcgaOverlayState state_ =
        MillenniumDosVideoFunctionSixMcgaOverlayState::awaiting_stack_push;
};

} // namespace eon
