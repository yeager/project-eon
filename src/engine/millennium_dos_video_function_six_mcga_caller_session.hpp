#pragma once

#include "engine/millennium_dos_video_function_six_mcga_copy_loop_session.hpp"
#include "engine/millennium_dos_video_function_six_mcga_helper_session.hpp"
#include "engine/millennium_dos_video_function_six_mcga_overlay_loop_session.hpp"
#include "engine/millennium_dos_video_function_six_mcga_register_setup_session.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace eon {

struct MillenniumDosVideoFunctionSixMcgaCallerStackRead {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    MillenniumDosVideoFunctionSixMcgaHelperStackSegment ss;
    std::uint16_t offset = 0;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaCallerStackRead&) const = default;
};

struct MillenniumDosVideoFunctionSixMcgaCallerCodeByteRead {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    std::uint16_t cs = 0;
    std::uint16_t offset = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaCallerCodeByteRead&) const = default;
};

enum class MillenniumDosVideoFunctionSixMcgaCallerState {
    awaiting_caller_return,
    awaiting_stack_restore,
    awaiting_mode_byte,
    awaiting_copy_return,
    complete,
    before_overlay_branch,
};

struct MillenniumDosVideoFunctionSixMcgaCallerOutcome {
    std::uint16_t instruction_boundary = 0;
    std::uint16_t final_sp = 0;
    std::optional<std::uint16_t> caller_return_ip;
    std::optional<MillenniumDosVideoFunctionSixMcgaCopyLoopOutcome> copy_loop;
    MillenniumDosVideoFunctionSixMcgaHelperStackWordWrite call_stack_write;
    std::vector<MillenniumDosVideoFunctionSixMcgaCallerStackRead> stack_reads;
    std::optional<MillenniumDosVideoFunctionSixMcgaCallerCodeByteRead> mode_read;
    std::optional<MillenniumDosVideoFunctionSixMcgaOverlayRegisters> overlay_registers;
};

// Connects the observed $073a setup outcome through CALL $0666 and the
// caller's stack restore, mode/parity dispatch, and bounded data movement.
// Byte effects are represented as observations, not pixel meanings.
class MillenniumDosVideoFunctionSixMcgaCallerSession {
public:
    MillenniumDosVideoFunctionSixMcgaCallerSession(
        std::span<const std::uint8_t> english_mcga_driver,
        const MillenniumDosVideoFunctionSixMcgaRegisterSetupOutcome& setup,
        const MillenniumDosVideoFunctionSixMcgaHelperOutcome& helper,
        bool direction_flag,
        std::uint32_t max_rows = 4096);

    [[nodiscard]] MillenniumDosVideoFunctionSixMcgaCallerState state() const { return state_; }
    [[nodiscard]] const std::optional<MillenniumDosVideoFunctionSixMcgaCallerOutcome>& outcome() const {
        return outcome_;
    }
    [[nodiscard]] const std::vector<MillenniumDosVideoFunctionSixMcgaCallerStackRead>& stack_reads() const {
        return stack_reads_;
    }

    void observe_stack_read(const MillenniumDosVideoFunctionSixMcgaCallerStackRead& read);
    void observe_mode_byte(const MillenniumDosVideoFunctionSixMcgaCallerCodeByteRead& read,
        std::span<const std::uint8_t> english_mcga_driver);

private:
    void finish(std::uint16_t instruction, std::uint16_t return_ip, std::uint16_t sp);

    MillenniumDosVideoFunctionSixMcgaRegisterSetupOutcome setup_;
    MillenniumDosVideoFunctionSixMcgaHelperOutcome helper_;
    bool direction_flag_ = false;
    std::uint32_t max_rows_ = 0;
    MillenniumDosVideoFunctionSixMcgaCallerState state_ =
        MillenniumDosVideoFunctionSixMcgaCallerState::awaiting_stack_restore;
    std::uint64_t last_sequence_ = 0;
    std::uint16_t sp_ = 0;
    std::vector<MillenniumDosVideoFunctionSixMcgaCallerStackRead> stack_reads_;
    std::optional<MillenniumDosVideoFunctionSixMcgaCallerCodeByteRead> mode_read_;
    std::optional<MillenniumDosVideoFunctionSixMcgaCopyLoopOutcome> copy_loop_;
    std::optional<MillenniumDosVideoFunctionSixMcgaCallerOutcome> outcome_;
    MillenniumDosVideoFunctionSixMcgaHelperStackWordWrite call_stack_write_;
};

} // namespace eon
