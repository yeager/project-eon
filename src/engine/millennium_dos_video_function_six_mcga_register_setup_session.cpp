#include "engine/millennium_dos_video_function_six_mcga_register_setup_session.hpp"

#include "data/millennium_dos_video_driver.hpp"
#include "data/sha256.hpp"

#include <stdexcept>
#include <utility>

namespace eon {

MillenniumDosVideoFunctionSixMcgaRegisterSetupSession::MillenniumDosVideoFunctionSixMcgaRegisterSetupSession(
    const std::span<const std::uint8_t> english_mcga_driver,
    const MillenniumDosVideoFunctionSixMcgaRegisterSnapshot initial_registers)
    : initial_registers_(initial_registers), registers_(initial_registers) {
    constexpr std::size_t offset = 0x073a;
    constexpr std::size_t span_size = 0x35;
    if (initial_registers.bx > 0xffeeU) {
        throw std::runtime_error("MCGA function-$06 descriptor words would wrap");
    }
    if (english_mcga_driver.size() != 4'366 || offset > english_mcga_driver.size()
        || english_mcga_driver.size() - offset < span_size
        || to_hex(sha256(english_mcga_driver.subspan(offset, span_size)))
            != "0ca0880330e05646d246c3031d6751066f50e1c282f5f389ade1009571a880a4") {
        throw std::runtime_error("Unsupported English MCGA function-$06 register setup span");
    }
    const auto profile = parse_millennium_dos_video_driver(
        english_mcga_driver, MillenniumDosVideoDriverKind::mcga);
    if (profile.function_six_address != 0x0705) {
        throw std::runtime_error("Unexpected MCGA function-$06 entry");
    }

    // PUSH DS executes before MOV DS,CX. The 16-bit stack pointer wraps as on
    // the target CPU; record the exact SS:SP write as a derived effect.
    push(0x073a, initial_registers.ds);
    registers_.ds = initial_registers.cx;
}

void MillenniumDosVideoFunctionSixMcgaRegisterSetupSession::push(
    const std::uint16_t instruction_address, const std::uint16_t value) {
    registers_.sp = static_cast<std::uint16_t>(registers_.sp - 2U);
    stack_writes_.push_back({instruction_address, registers_.ss, registers_.sp, value});
}

void MillenniumDosVideoFunctionSixMcgaRegisterSetupSession::require_read(
    const std::uint64_t sequence, const std::uint16_t instruction_address,
    const std::uint16_t field_offset) const {
    if (sequence != last_sequence_ + 1 || registers_.bx > 0xffeeU
        || instruction_address == 0 || field_offset > 0x10U
        || static_cast<std::uint32_t>(registers_.bx) + field_offset > 0xffffU) {
        throw std::runtime_error("MCGA function-$06 descriptor observation order/offset mismatch");
    }
}

void MillenniumDosVideoFunctionSixMcgaRegisterSetupSession::finish() {
    outcome_ = MillenniumDosVideoFunctionSixMcgaRegisterSetupOutcome{
        initial_registers_, registers_, word_reads_, byte_reads_, stack_writes_};
    state_ = MillenniumDosVideoFunctionSixMcgaRegisterSetupState::before_helper_call;
}

void MillenniumDosVideoFunctionSixMcgaRegisterSetupSession::observe_word_read(
    const MillenniumDosVideoFunctionSixMcgaDescriptorRead& read) {
    std::uint16_t expected_instruction = 0;
    std::uint16_t field = 0;
    switch (state_) {
    case MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_signed_multiply_operand:
        expected_instruction = 0x073d; field = 0x0a; break;
    case MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_ax_addend:
        expected_instruction = 0x0741; field = 0x0c; break;
    case MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_position_addend:
        expected_instruction = 0x0755; field = 0x08; break;
    case MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_width_word:
        expected_instruction = 0x0759; field = 0x10; break;
    case MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_height_word:
        expected_instruction = 0x0764; field = 0x0e; break;
    case MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_ah_byte:
    case MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_final_byte:
    case MillenniumDosVideoFunctionSixMcgaRegisterSetupState::before_helper_call:
        throw std::runtime_error("MCGA function-$06 word read at byte/terminal boundary");
    }
    require_read(read.sequence, expected_instruction, field);
    if (read.instruction_address != expected_instruction || read.es != registers_.es
        || read.offset != static_cast<std::uint16_t>(registers_.bx + field)) {
        throw std::runtime_error("MCGA function-$06 descriptor word mismatch");
    }
    last_sequence_ = read.sequence;
    word_reads_.push_back(read);

    switch (state_) {
    case MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_signed_multiply_operand: {
        multiply_operand_ = read.value;
        const auto signed_word = [](const std::uint16_t value) {
            return value < 0x8000U
                ? static_cast<std::int32_t>(value)
                : static_cast<std::int32_t>(value) - 0x1'0000;
        };
        const auto product = signed_word(registers_.ax) * signed_word(multiply_operand_);
        const auto product_bits = static_cast<std::uint32_t>(product);
        registers_.ax = static_cast<std::uint16_t>(product_bits & 0xffffU);
        registers_.dx = static_cast<std::uint16_t>((product_bits >> 16U) & 0xffffU);
        state_ = MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_ax_addend;
        break;
    }
    case MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_ax_addend:
        ax_addend_ = read.value;
        registers_.ax = static_cast<std::uint16_t>(registers_.ax + ax_addend_);
        registers_.si = static_cast<std::uint16_t>(registers_.si + registers_.ax);
        state_ = MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_ah_byte;
        break;
    case MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_position_addend:
        position_addend_ = read.value;
        registers_.ax = static_cast<std::uint16_t>(registers_.ax + position_addend_);
        state_ = MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_width_word;
        break;
    case MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_width_word:
        width_word_ = read.value;
        registers_.dx = width_word_;
        push(0x075d, registers_.dx);
        registers_.dx = static_cast<std::uint16_t>(registers_.dx - 0x0140U);
        registers_.dx = static_cast<std::uint16_t>(0U - registers_.dx);
        state_ = MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_height_word;
        break;
    case MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_height_word:
        height_word_ = read.value;
        push(0x0764, height_word_);
        push(0x0768, registers_.di);
        std::swap(registers_.ax, registers_.di);
        push(0x076a, registers_.dx);
        state_ = MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_final_byte;
        break;
    case MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_ah_byte:
    case MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_final_byte:
    case MillenniumDosVideoFunctionSixMcgaRegisterSetupState::before_helper_call:
        throw std::runtime_error("MCGA function-$06 invalid word-read transition");
    }
}

void MillenniumDosVideoFunctionSixMcgaRegisterSetupSession::observe_byte_read(
    const MillenniumDosVideoFunctionSixMcgaDescriptorByteRead& read) {
    std::uint16_t expected_instruction = 0;
    std::uint16_t field = 0;
    switch (state_) {
    case MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_ah_byte:
        expected_instruction = 0x0747; field = 0x06; break;
    case MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_final_byte:
        expected_instruction = 0x076b; field = 0x04; break;
    case MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_signed_multiply_operand:
    case MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_ax_addend:
    case MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_position_addend:
    case MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_width_word:
    case MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_height_word:
    case MillenniumDosVideoFunctionSixMcgaRegisterSetupState::before_helper_call:
        throw std::runtime_error("MCGA function-$06 byte read at word/terminal boundary");
    }
    require_read(read.sequence, expected_instruction, field);
    if (read.instruction_address != expected_instruction || read.es != registers_.es
        || read.offset != static_cast<std::uint16_t>(registers_.bx + field)) {
        throw std::runtime_error("MCGA function-$06 descriptor byte mismatch");
    }
    last_sequence_ = read.sequence;
    byte_reads_.push_back(read);
    if (state_ == MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_ah_byte) {
        ah_byte_ = read.value;
        registers_.ax = static_cast<std::uint16_t>((registers_.ax & 0x00ffU)
            | static_cast<std::uint16_t>(static_cast<std::uint16_t>(ah_byte_) << 8U));
        registers_.ax = static_cast<std::uint16_t>(registers_.ax & 0xff00U);
        registers_.dx = registers_.ax;
        registers_.ax = static_cast<std::uint16_t>(registers_.ax >> 1U);
        registers_.ax = static_cast<std::uint16_t>(registers_.ax >> 1U);
        registers_.ax = static_cast<std::uint16_t>(registers_.ax + registers_.dx);
        state_ = MillenniumDosVideoFunctionSixMcgaRegisterSetupState::awaiting_position_addend;
        return;
    }
    registers_.ax = static_cast<std::uint16_t>((registers_.ax & 0xff00U) | read.value);
    finish();
}

} // namespace eon
