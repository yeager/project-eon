#include "engine/millennium_dos_video_function_six_mcga_caller_session.hpp"

#include "data/millennium_dos_video_driver.hpp"
#include "data/sha256.hpp"

#include <stdexcept>
#include <string_view>

namespace eon {

MillenniumDosVideoFunctionSixMcgaCallerSession::MillenniumDosVideoFunctionSixMcgaCallerSession(
    const std::span<const std::uint8_t> english_mcga_driver,
    const MillenniumDosVideoFunctionSixMcgaRegisterSetupOutcome& setup,
    const MillenniumDosVideoFunctionSixMcgaHelperOutcome& helper,
    const bool direction_flag, const std::uint32_t max_rows)
    : setup_(setup), helper_(helper),
      direction_flag_(direction_flag), max_rows_(max_rows), sp_(helper.final_sp) {
    constexpr std::string_view driver_hash =
        "bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228";
    if (english_mcga_driver.size() != 4'366 || to_hex(sha256(english_mcga_driver)) != driver_hash) {
        throw std::runtime_error("Unsupported English MCGA driver for function-six caller continuation");
    }
    static_cast<void>(parse_millennium_dos_video_driver(
        english_mcga_driver, MillenniumDosVideoDriverKind::mcga));
    if (max_rows == 0 || max_rows > 4096) {
        throw std::runtime_error("MCGA caller copy-loop row bound is outside the supported limit");
    }

    const auto& initial = setup.initial_registers;
    const auto& prepared = setup.final_registers;
    const auto setup_push_sp = static_cast<std::uint16_t>(initial.sp - 2U);
    const auto call_sp = static_cast<std::uint16_t>(prepared.sp - 2U);
    if (setup.stack_writes.size() != 5 || setup.stack_writes[0]
            != MillenniumDosVideoFunctionSixMcgaSetupStackWrite{0x073a, initial.ss, setup_push_sp, initial.ds}
        || setup.stack_writes[1].instruction_address != 0x075d
        || setup.stack_writes[1].ss != initial.ss
        || setup.stack_writes[1].offset != static_cast<std::uint16_t>(setup_push_sp - 2U)
        || setup.stack_writes[2].instruction_address != 0x0764
        || setup.stack_writes[2].ss != initial.ss
        || setup.stack_writes[2].offset != static_cast<std::uint16_t>(setup_push_sp - 4U)
        || setup.stack_writes[3].instruction_address != 0x0768
        || setup.stack_writes[3].ss != initial.ss
        || setup.stack_writes[3].offset != static_cast<std::uint16_t>(setup_push_sp - 6U)
        || setup.stack_writes[4].instruction_address != 0x076a
        || setup.stack_writes[4].ss != initial.ss
        || setup.stack_writes[4].offset != static_cast<std::uint16_t>(setup_push_sp - 8U)
        || prepared.sp != static_cast<std::uint16_t>(initial.sp - 10U)
        || helper.initial_ax != prepared.ax || helper.initial_bx != prepared.bx
        || helper.initial_dx != prepared.dx || helper.ds.value != prepared.ds
        || helper.ss.value != initial.ss || helper.initial_sp != call_sp
        || helper.return_ip != 0x0772 || helper.final_sp != prepared.sp) {
        throw std::runtime_error("MCGA function-six setup/helper observations do not form the $076f call boundary");
    }
    state_ = helper.carry
        ? MillenniumDosVideoFunctionSixMcgaCallerState::awaiting_caller_return
        : MillenniumDosVideoFunctionSixMcgaCallerState::awaiting_stack_restore;
    call_stack_write_ = {0x076f, helper.ss, call_sp, 0x0772};
}

void MillenniumDosVideoFunctionSixMcgaCallerSession::observe_stack_read(
    const MillenniumDosVideoFunctionSixMcgaCallerStackRead& read) {
    if (state_ == MillenniumDosVideoFunctionSixMcgaCallerState::awaiting_caller_return) {
        if (read.sequence != last_sequence_ + 1 || read.instruction_address != 0x07a0
            || read.ss.value != setup_.initial_registers.ss || read.offset != setup_.initial_registers.sp) {
            throw std::runtime_error("MCGA function-six failed-helper caller RET observation mismatch");
        }
        last_sequence_ = read.sequence;
        stack_reads_.push_back(read);
        finish(0x07a0, read.value, static_cast<std::uint16_t>(read.offset + 2U));
        return;
    }

    if (state_ == MillenniumDosVideoFunctionSixMcgaCallerState::awaiting_copy_return) {
        const auto ret_address = copy_loop_->next_instruction;
        if (read.sequence != last_sequence_ + 1 || read.instruction_address != ret_address
            || read.ss.value != setup_.initial_registers.ss || read.offset != setup_.initial_registers.sp) {
            throw std::runtime_error("MCGA function-six copy-loop RET observation mismatch");
        }
        last_sequence_ = read.sequence;
        stack_reads_.push_back(read);
        finish(ret_address, read.value, static_cast<std::uint16_t>(read.offset + 2U));
        return;
    }

    if (state_ != MillenniumDosVideoFunctionSixMcgaCallerState::awaiting_stack_restore
        || stack_reads_.size() >= 5 || read.sequence != last_sequence_ + 1
        || read.ss.value != setup_.initial_registers.ss) {
        throw std::runtime_error("MCGA function-six caller stack restore read out of order");
    }
    constexpr std::uint16_t pop_addresses[] = {0x0778, 0x0779, 0x077a, 0x077b, 0x077c};
    const auto index = stack_reads_.size();
    const auto expected_offset = static_cast<std::uint16_t>(setup_.final_registers.sp + 2U * index);
    if (read.instruction_address != pop_addresses[index] || read.offset != expected_offset) {
        throw std::runtime_error("MCGA function-six caller stack restore address mismatch");
    }
    last_sequence_ = read.sequence;
    stack_reads_.push_back(read);
    sp_ = static_cast<std::uint16_t>(read.offset + 2U);
    if (stack_reads_.size() == 5) state_ = MillenniumDosVideoFunctionSixMcgaCallerState::awaiting_mode_byte;
}

void MillenniumDosVideoFunctionSixMcgaCallerSession::observe_mode_byte(
    const MillenniumDosVideoFunctionSixMcgaCallerCodeByteRead& read,
    const std::span<const std::uint8_t> english_mcga_driver) {
    if (state_ != MillenniumDosVideoFunctionSixMcgaCallerState::awaiting_mode_byte
        || read.sequence != last_sequence_ + 1 || read.instruction_address != 0x077f
        || read.offset != 0x07b9) {
        throw std::runtime_error("MCGA function-six caller CS:$07b9 mode observation mismatch");
    }
    if (english_mcga_driver.size() != 4'366
        || to_hex(sha256(english_mcga_driver))
            != "bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228") {
        throw std::runtime_error("Unsupported English MCGA driver at function-six mode dispatch");
    }
    last_sequence_ = read.sequence;
    mode_read_ = read;
    if (read.value != 0x88) {
        state_ = MillenniumDosVideoFunctionSixMcgaCallerState::before_overlay_branch;
        const auto& initial = setup_.final_registers;
        const auto restored_dx = stack_reads_[0].value;
        const auto restored_bx = static_cast<std::uint16_t>(stack_reads_[1].value
            - stack_reads_[3].value);
        const auto restored_bp = stack_reads_[2].value;
        const auto restored_ax = stack_reads_[3].value;
        const auto restored_ds = stack_reads_[4].value;
        const auto restored_di = static_cast<std::uint16_t>(initial.di + helper_.final_ax);
        const MillenniumDosVideoFunctionSixMcgaOverlayRegisters overlay_registers{
            initial.cx, restored_bx, restored_ax, restored_dx, initial.si, restored_di,
            restored_bp, restored_ds, helper_.final_dx, setup_.initial_registers.ss,
            sp_, direction_flag_};
        outcome_ = MillenniumDosVideoFunctionSixMcgaCallerOutcome{
            0x07b5, sp_, std::nullopt, std::nullopt, call_stack_write_, stack_reads_,
            mode_read_, overlay_registers};
        return;
    }

    const auto& regs = setup_.final_registers;
    // Caller instructions: MOV ES,DX; ADD DI,AX; then POP DX,BX,BP,AX,DS;
    // SUB BX,AX. All five popped words remain explicit observations.
    const auto restored_dx = stack_reads_[0].value;
    const auto restored_bx = static_cast<std::uint16_t>(stack_reads_[1].value
        - stack_reads_[3].value);
    const auto restored_bp = stack_reads_[2].value;
    const auto restored_ax = stack_reads_[3].value;
    const auto restored_ds = stack_reads_[4].value;
    const auto restored_di = static_cast<std::uint16_t>(regs.di + helper_.final_ax);
    const auto loop_kind = (restored_ax & 1U) == 0
        ? MillenniumDosVideoFunctionSixMcgaCopyLoopKind::word_aligned
        : MillenniumDosVideoFunctionSixMcgaCopyLoopKind::byte_aligned;
    const MillenniumDosVideoFunctionSixMcgaCopyLoopRegisters loop_registers{
        restored_ax, restored_bx, regs.cx, restored_dx, regs.si, restored_di,
        restored_bp, restored_ds, helper_.final_dx, direction_flag_};
    MillenniumDosVideoFunctionSixMcgaCopyLoopSession loop(
        english_mcga_driver, loop_kind, loop_registers, max_rows_);
    copy_loop_ = loop.outcome();
    state_ = MillenniumDosVideoFunctionSixMcgaCallerState::awaiting_copy_return;
}

void MillenniumDosVideoFunctionSixMcgaCallerSession::finish(
    const std::uint16_t instruction, const std::uint16_t return_ip, const std::uint16_t sp) {
    outcome_ = MillenniumDosVideoFunctionSixMcgaCallerOutcome{
        instruction, sp, return_ip, copy_loop_, call_stack_write_, stack_reads_, mode_read_, std::nullopt};
    state_ = MillenniumDosVideoFunctionSixMcgaCallerState::complete;
}

} // namespace eon
