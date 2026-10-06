#pragma once

#include "engine/millennium_dos_video_function_six_mcga_table_session.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace eon {

struct MillenniumDosVideoFunctionSixMcgaHelperStackSegment {
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaHelperStackSegment&) const = default;
};

struct MillenniumDosVideoFunctionSixMcgaHelperStackWordWrite {
    std::uint16_t instruction_address = 0;
    MillenniumDosVideoFunctionSixMcgaHelperStackSegment ss;
    std::uint16_t offset = 0;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaHelperStackWordWrite&) const = default;
};

struct MillenniumDosVideoFunctionSixMcgaHelperStackWordRead {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    MillenniumDosVideoFunctionSixMcgaHelperStackSegment ss;
    std::uint16_t offset = 0;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaHelperStackWordRead&) const = default;
};

struct MillenniumDosVideoFunctionSixMcgaHelperDataByteRead {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    MillenniumDosVideoDataSegment ds;
    std::uint16_t offset = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaHelperDataByteRead&) const = default;
};

struct MillenniumDosVideoFunctionSixMcgaHelperTableWordRead {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    MillenniumDosVideoDataSegment ds;
    std::uint16_t offset = 0;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaHelperTableWordRead&) const = default;
};

struct MillenniumDosVideoFunctionSixMcgaHelperOutcome {
    std::uint16_t initial_ax = 0;
    std::uint16_t initial_bx = 0;
    std::uint16_t initial_dx = 0;
    MillenniumDosVideoDataSegment ds;
    MillenniumDosVideoFunctionSixMcgaHelperStackSegment ss;
    std::uint16_t initial_sp = 0;
    std::uint16_t final_ax = 0;
    std::uint16_t final_bx = 0;
    std::uint16_t final_dx = 0;
    std::uint16_t final_sp = 0;
    bool carry = false;
    std::uint16_t return_ip = 0;
    std::optional<std::uint16_t> table_offset;
    std::vector<MillenniumDosVideoFunctionSixMcgaHelperTableWordRead> table_reads;
    std::vector<MillenniumDosVideoFunctionSixMcgaHelperStackWordWrite> stack_writes;
    std::vector<MillenniumDosVideoFunctionSixMcgaHelperStackWordRead> stack_reads;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaHelperOutcome&) const = default;
};

enum class MillenniumDosVideoFunctionSixMcgaHelperState {
    awaiting_threshold,
    awaiting_table_word_zero,
    awaiting_table_word_two,
    awaiting_bx_pop,
    awaiting_ret,
    complete,
};

// Standalone, hash-bound facts for the English MCGA helper at [$0666,$0681).
// Its bytes specify only observations/register/stack effects; this session has
// no assumed caller, lookup-data interpretation, or blit behavior.
class MillenniumDosVideoFunctionSixMcgaHelperSession {
public:
    MillenniumDosVideoFunctionSixMcgaHelperSession(
        std::span<const std::uint8_t> english_mcga_driver,
        std::uint16_t ax, std::uint16_t bx, std::uint16_t dx,
        MillenniumDosVideoDataSegment ds, MillenniumDosVideoFunctionSixMcgaHelperStackSegment ss,
        std::uint16_t sp);

    [[nodiscard]] MillenniumDosVideoFunctionSixMcgaHelperState state() const { return state_; }
    [[nodiscard]] const std::optional<MillenniumDosVideoFunctionSixMcgaHelperOutcome>& outcome() const {
        return outcome_;
    }
    [[nodiscard]] const std::vector<MillenniumDosVideoFunctionSixMcgaHelperStackWordWrite>& stack_writes() const {
        return stack_writes_;
    }

    void observe_threshold(const MillenniumDosVideoFunctionSixMcgaHelperDataByteRead& read);
    void observe_table_word(const MillenniumDosVideoFunctionSixMcgaHelperTableWordRead& read);
    void observe_stack_word(const MillenniumDosVideoFunctionSixMcgaHelperStackWordRead& read);

private:
    void finish(std::uint16_t return_ip);

    std::uint16_t initial_ax_ = 0;
    std::uint16_t initial_bx_ = 0;
    std::uint16_t initial_dx_ = 0;
    std::uint16_t ax_ = 0;
    std::uint16_t bx_ = 0;
    std::uint16_t dx_ = 0;
    MillenniumDosVideoDataSegment ds_;
    MillenniumDosVideoFunctionSixMcgaHelperStackSegment ss_;
    std::uint16_t initial_sp_ = 0;
    std::uint16_t sp_ = 0;
    MillenniumDosVideoFunctionSixMcgaHelperState state_ =
        MillenniumDosVideoFunctionSixMcgaHelperState::awaiting_threshold;
    std::uint64_t last_sequence_ = 0;
    bool carry_ = false;
    std::uint16_t table_offset_ = 0;
    std::vector<MillenniumDosVideoFunctionSixMcgaHelperTableWordRead> table_reads_;
    std::vector<MillenniumDosVideoFunctionSixMcgaHelperStackWordWrite> stack_writes_;
    std::vector<MillenniumDosVideoFunctionSixMcgaHelperStackWordRead> stack_reads_;
    std::optional<MillenniumDosVideoFunctionSixMcgaHelperOutcome> outcome_;
};

} // namespace eon
