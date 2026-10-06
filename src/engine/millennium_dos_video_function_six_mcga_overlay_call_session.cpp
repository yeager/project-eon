#include "engine/millennium_dos_video_function_six_mcga_overlay_call_session.hpp"

#include <stdexcept>

namespace eon {

MillenniumDosVideoFunctionSixMcgaOverlayCallSession::MillenniumDosVideoFunctionSixMcgaOverlayCallSession(
    const std::span<const std::uint8_t> english_mcga_driver,
    const MillenniumDosVideoFunctionSixMcgaCallerOutcome& caller,
    const std::uint32_t max_rows,
    const std::uint32_t max_total_bytes)
    : branch_byte_(caller.mode_read ? caller.mode_read->value : 0),
      caller_ss_(caller.overlay_registers ? caller.overlay_registers->ss : 0),
      caller_sp_(caller.final_sp),
      preceding_sequence_(caller.mode_read ? caller.mode_read->sequence : 0),
      loop_(english_mcga_driver,
          caller.overlay_registers.value_or(MillenniumDosVideoFunctionSixMcgaOverlayRegisters{}),
          max_rows, max_total_bytes, preceding_sequence_) {
    constexpr std::uint16_t stack_sites[] = {0x0778,0x0779,0x077a,0x077b,0x077c};
    bool valid_stack_reads = caller.stack_reads.size() == 5;
    for (std::size_t i = 0; valid_stack_reads && i < 5; ++i) {
        valid_stack_reads = caller.stack_reads[i].instruction_address == stack_sites[i]
            && caller.stack_reads[i].ss.value == caller.overlay_registers.value_or(
                MillenniumDosVideoFunctionSixMcgaOverlayRegisters{}).ss
            && (i == 0 || caller.stack_reads[i].sequence > caller.stack_reads[i - 1].sequence);
    }
    if (caller.instruction_boundary != 0x07b5 || caller.caller_return_ip
        || caller.copy_loop || !caller.overlay_registers
        || caller.final_sp != caller.overlay_registers->sp
        || !valid_stack_reads || !caller.mode_read
        || caller.mode_read->instruction_address != 0x077f
        || caller.mode_read->offset != 0x07b9
        || caller.mode_read->sequence <= caller.stack_reads.back().sequence
        || caller.mode_read->value == 0x88
        || caller.call_stack_write.instruction_address != 0x076f
        || caller.call_stack_write.value != 0x0772
        || caller.call_stack_write.ss.value != caller.overlay_registers->ss) {
        throw std::runtime_error("MCGA overlay composition requires the explicit non-$88 caller boundary");
    }
}

void MillenniumDosVideoFunctionSixMcgaOverlayCallSession::observe_caller_return(
    const MillenniumDosVideoFunctionSixMcgaOverlayCallReturn& value) {
    if (loop_.state() != MillenniumDosVideoFunctionSixMcgaOverlayState::complete || !loop_.outcome()
        || value.instruction_address != 0x07c6 || value.ss != caller_ss_
        || value.offset != caller_sp_ || value.sequence != loop_.last_sequence() + 1) {
        throw std::runtime_error("MCGA overlay RET return-word observation mismatch");
    }
    const auto& loop_outcome = *loop_.outcome();
    if (loop_outcome.final_registers.sp != caller_sp_) {
        throw std::runtime_error("MCGA overlay loop did not restore the caller return-word offset");
    }
    outcome_ = MillenniumDosVideoFunctionSixMcgaOverlayCallOutcome{
        branch_byte_, loop_outcome, value.return_ip,
        static_cast<std::uint16_t>(caller_sp_ + 2U)};
}

} // namespace eon
