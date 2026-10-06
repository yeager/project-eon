#pragma once

#include "engine/millennium_dos_video_function_six_mcga_caller_session.hpp"

namespace eon {

struct MillenniumDosVideoFunctionSixMcgaOverlayCallReturn {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0x07c6;
    std::uint16_t ss = 0;
    std::uint16_t offset = 0;
    std::uint16_t return_ip = 0;
};

struct MillenniumDosVideoFunctionSixMcgaOverlayCallOutcome {
    std::uint8_t branch_byte = 0;
    MillenniumDosVideoFunctionSixMcgaOverlayOutcome loop;
    std::uint16_t return_ip = 0;
    std::uint16_t final_sp = 0;
};

// Composes only the explicit non-$88 caller branch, overlay loop, and its RET.
// It assigns no INT91, graphics, or device semantics and proves no reachability.
class MillenniumDosVideoFunctionSixMcgaOverlayCallSession {
public:
    MillenniumDosVideoFunctionSixMcgaOverlayCallSession(
        std::span<const std::uint8_t> english_mcga_driver,
        const MillenniumDosVideoFunctionSixMcgaCallerOutcome& caller,
        std::uint32_t max_rows = 4096,
        std::uint32_t max_total_bytes = 1'048'576);

    [[nodiscard]] MillenniumDosVideoFunctionSixMcgaOverlayState state() const { return loop_.state(); }
    [[nodiscard]] const std::optional<MillenniumDosVideoFunctionSixMcgaOverlayCallOutcome>& outcome() const {
        return outcome_;
    }
    void observe_stack_write(const MillenniumDosVideoFunctionSixMcgaOverlayStackWrite& value) {
        loop_.observe_stack_write(value);
    }
    void observe_source_read(const MillenniumDosVideoFunctionSixMcgaOverlaySourceRead& value) {
        loop_.observe_source_read(value);
    }
    void observe_destination_read(const MillenniumDosVideoFunctionSixMcgaOverlayDestinationRead& value) {
        loop_.observe_destination_read(value);
    }
    void observe_destination_write(const MillenniumDosVideoFunctionSixMcgaOverlayDestinationWrite& value) {
        loop_.observe_destination_write(value);
    }
    void observe_stack_read(const MillenniumDosVideoFunctionSixMcgaOverlayStackRead& value) {
        loop_.observe_stack_read(value);
    }
    void observe_caller_return(const MillenniumDosVideoFunctionSixMcgaOverlayCallReturn& value);

private:
    std::uint8_t branch_byte_ = 0;
    std::uint16_t caller_ss_ = 0;
    std::uint16_t caller_sp_ = 0;
    std::uint64_t preceding_sequence_ = 0;
    MillenniumDosVideoFunctionSixMcgaOverlayLoopSession loop_;
    std::optional<MillenniumDosVideoFunctionSixMcgaOverlayCallOutcome> outcome_;
};

} // namespace eon
