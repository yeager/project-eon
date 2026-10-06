#pragma once

#include "engine/millennium_dos_video_function_six_mcga_descriptor_byte_read.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace eon {

struct MillenniumDosVideoFunctionSixMcgaRegisterSnapshot {
    std::uint16_t ax = 0;
    std::uint16_t bx = 0;
    std::uint16_t cx = 0;
    std::uint16_t dx = 0;
    std::uint16_t si = 0;
    std::uint16_t di = 0;
    std::uint16_t ds = 0;
    std::uint16_t es = 0;
    std::uint16_t ss = 0;
    std::uint16_t sp = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaRegisterSnapshot&) const = default;
};

struct MillenniumDosVideoFunctionSixMcgaDescriptorRead {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    std::uint16_t es = 0;
    std::uint16_t offset = 0;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaDescriptorRead&) const = default;
};

struct MillenniumDosVideoFunctionSixMcgaSetupStackWrite {
    std::uint16_t instruction_address = 0;
    std::uint16_t ss = 0;
    std::uint16_t offset = 0;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaSetupStackWrite&) const = default;
};

enum class MillenniumDosVideoFunctionSixMcgaRegisterSetupState {
    awaiting_signed_multiply_operand,
    awaiting_ax_addend,
    awaiting_ah_byte,
    awaiting_position_addend,
    awaiting_width_word,
    awaiting_height_word,
    awaiting_final_byte,
    before_helper_call,
};

struct MillenniumDosVideoFunctionSixMcgaRegisterSetupOutcome {
    MillenniumDosVideoFunctionSixMcgaRegisterSnapshot initial_registers;
    MillenniumDosVideoFunctionSixMcgaRegisterSnapshot final_registers;
    std::vector<MillenniumDosVideoFunctionSixMcgaDescriptorRead> word_reads;
    std::vector<MillenniumDosVideoFunctionSixMcgaDescriptorByteRead> byte_reads;
    std::vector<MillenniumDosVideoFunctionSixMcgaSetupStackWrite> stack_writes;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaRegisterSetupOutcome&) const = default;
};

// Standalone hash-bound 16-bit register/stack observation for MCGA.BIN
// [$073a,$076f). Inputs are an explicit register snapshot and ordered raw
// descriptor observations. It has no assumed predecessor and stops before
// CALL $0666; it does not infer the helper's meaning or any blit.
class MillenniumDosVideoFunctionSixMcgaRegisterSetupSession {
public:
    MillenniumDosVideoFunctionSixMcgaRegisterSetupSession(
        std::span<const std::uint8_t> english_mcga_driver,
        MillenniumDosVideoFunctionSixMcgaRegisterSnapshot initial_registers);

    [[nodiscard]] MillenniumDosVideoFunctionSixMcgaRegisterSetupState state() const { return state_; }
    [[nodiscard]] const MillenniumDosVideoFunctionSixMcgaRegisterSnapshot& registers() const {
        return registers_;
    }
    [[nodiscard]] const std::vector<MillenniumDosVideoFunctionSixMcgaSetupStackWrite>& stack_writes() const {
        return stack_writes_;
    }
    [[nodiscard]] const std::optional<MillenniumDosVideoFunctionSixMcgaRegisterSetupOutcome>& outcome() const {
        return outcome_;
    }

    void observe_word_read(const MillenniumDosVideoFunctionSixMcgaDescriptorRead& read);
    void observe_byte_read(const MillenniumDosVideoFunctionSixMcgaDescriptorByteRead& read);

private:
    void push(std::uint16_t instruction_address, std::uint16_t value);
    void require_read(std::uint64_t sequence, std::uint16_t instruction_address,
        std::uint16_t offset) const;
    void finish();

    MillenniumDosVideoFunctionSixMcgaRegisterSnapshot initial_registers_;
    MillenniumDosVideoFunctionSixMcgaRegisterSnapshot registers_;
    MillenniumDosVideoFunctionSixMcgaRegisterSetupState state_ =
        MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_signed_multiply_operand;
    std::uint64_t last_sequence_ = 0;
    std::uint16_t multiply_operand_ = 0;
    std::uint16_t ax_addend_ = 0;
    std::uint8_t ah_byte_ = 0;
    std::uint16_t position_addend_ = 0;
    std::uint16_t width_word_ = 0;
    std::uint16_t height_word_ = 0;
    std::vector<MillenniumDosVideoFunctionSixMcgaDescriptorRead> word_reads_;
    std::vector<MillenniumDosVideoFunctionSixMcgaDescriptorByteRead> byte_reads_;
    std::vector<MillenniumDosVideoFunctionSixMcgaSetupStackWrite> stack_writes_;
    std::optional<MillenniumDosVideoFunctionSixMcgaRegisterSetupOutcome> outcome_;
};

} // namespace eon
