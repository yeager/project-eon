#include "engine/millennium_dos_video_function_six_mcga_pointer_setup_session.hpp"

#include "data/sha256.hpp"

#include <stdexcept>

namespace eon {

MillenniumDosVideoFunctionSixMcgaPointerSetupSession::MillenniumDosVideoFunctionSixMcgaPointerSetupSession(
    const std::span<const std::uint8_t> english_mcga_driver,
    const std::uint16_t descriptor_es,
    const std::uint16_t descriptor_bx,
    const MillenniumDosVideoDataSegment initial_ds,
    const MillenniumDosVideoStackSegment ss,
    const std::uint16_t initial_sp)
    : descriptor_es_(descriptor_es), descriptor_bx_(descriptor_bx), initial_ds_(initial_ds),
      ss_(ss), initial_sp_(initial_sp),
      stack_offset_(static_cast<std::uint16_t>(initial_sp - 2U)),
      pushed_ds_{0x072d, ss, stack_offset_, initial_ds.value} {
    constexpr std::size_t offset = 0x072d;
    constexpr std::size_t span_size = 0x0d;
    if (descriptor_bx > 0xfffcU) {
        throw std::runtime_error("MCGA function-$06 descriptor far pointer would wrap");
    }
    if (english_mcga_driver.size() != 4'366 || offset > english_mcga_driver.size()
        || english_mcga_driver.size() - offset < span_size
        || to_hex(sha256(english_mcga_driver.subspan(offset, span_size)))
            != "dcdc5c863885078f886d34950293217ae1b92eb4374df134c26822412c577bce") {
        throw std::runtime_error("Unsupported English MCGA function-$06 pointer setup span");
    }
    static_cast<void>(parse_millennium_dos_video_driver(
        english_mcga_driver, MillenniumDosVideoDriverKind::mcga));
}

void MillenniumDosVideoFunctionSixMcgaPointerSetupSession::require_state(
    const MillenniumDosVideoFunctionSixMcgaPointerSetupState expected,
    const std::uint64_t sequence, const std::uint16_t instruction_address) const {
    if (state_ != expected || sequence != last_sequence_ + 1) {
        throw std::runtime_error("MCGA function-$06 pointer setup order mismatch");
    }
    const auto expected_instruction = state_ ==
        MillenniumDosVideoFunctionSixMcgaPointerSetupState::awaiting_descriptor_pointer_offset
        || state_ == MillenniumDosVideoFunctionSixMcgaPointerSetupState::awaiting_descriptor_pointer_segment
        ? 0x072e
        : state_ == MillenniumDosVideoFunctionSixMcgaPointerSetupState::awaiting_source_header_word
        ? 0x0731
        : state_ == MillenniumDosVideoFunctionSixMcgaPointerSetupState::awaiting_nested_pointer_offset
        || state_ == MillenniumDosVideoFunctionSixMcgaPointerSetupState::awaiting_nested_pointer_segment
        ? 0x0736 : 0x0739;
    if (instruction_address != expected_instruction) {
        throw std::runtime_error("MCGA function-$06 pointer setup instruction mismatch");
    }
}

void MillenniumDosVideoFunctionSixMcgaPointerSetupSession::observe_descriptor_pointer_offset(
    const MillenniumDosVideoFunctionSixMcgaDescriptorPointerWordRead& read) {
    require_state(MillenniumDosVideoFunctionSixMcgaPointerSetupState::awaiting_descriptor_pointer_offset,
        read.sequence, read.instruction_address);
    if (read.es != descriptor_es_ || read.offset != descriptor_bx_) {
        throw std::runtime_error("MCGA function-$06 descriptor pointer offset mismatch");
    }
    first_pointer_offset_ = read.value;
    descriptor_offset_read_ = read;
    last_sequence_ = read.sequence;
    state_ = MillenniumDosVideoFunctionSixMcgaPointerSetupState::awaiting_descriptor_pointer_segment;
}

void MillenniumDosVideoFunctionSixMcgaPointerSetupSession::observe_descriptor_pointer_segment(
    const MillenniumDosVideoFunctionSixMcgaDescriptorPointerWordRead& read) {
    require_state(MillenniumDosVideoFunctionSixMcgaPointerSetupState::awaiting_descriptor_pointer_segment,
        read.sequence, read.instruction_address);
    if (read.es != descriptor_es_ || read.offset != static_cast<std::uint16_t>(descriptor_bx_ + 2U)) {
        throw std::runtime_error("MCGA function-$06 descriptor pointer segment mismatch");
    }
    first_pointer_segment_ = {read.value};
    descriptor_segment_read_ = read;
    last_sequence_ = read.sequence;
    state_ = MillenniumDosVideoFunctionSixMcgaPointerSetupState::awaiting_source_header_word;
}

void MillenniumDosVideoFunctionSixMcgaPointerSetupSession::observe_source_header_word(
    const MillenniumDosVideoFunctionSixMcgaSourceWordRead& read) {
    require_state(MillenniumDosVideoFunctionSixMcgaPointerSetupState::awaiting_source_header_word,
        read.sequence, read.instruction_address);
    if (first_pointer_offset_ > 0xfffcU || read.ds != first_pointer_segment_
        || read.offset != static_cast<std::uint16_t>(first_pointer_offset_ + 2U)) {
        throw std::runtime_error("MCGA function-$06 first source word read mismatch");
    }
    header_word_ = read.value;
    source_header_read_ = read;
    last_sequence_ = read.sequence;
    state_ = MillenniumDosVideoFunctionSixMcgaPointerSetupState::awaiting_nested_pointer_offset;
}

void MillenniumDosVideoFunctionSixMcgaPointerSetupSession::observe_nested_pointer_offset(
    const MillenniumDosVideoFunctionSixMcgaSourceWordRead& read) {
    require_state(MillenniumDosVideoFunctionSixMcgaPointerSetupState::awaiting_nested_pointer_offset,
        read.sequence, read.instruction_address);
    if (first_pointer_offset_ > 0xfff8U || read.ds != first_pointer_segment_
        || read.offset != static_cast<std::uint16_t>(first_pointer_offset_ + 4U)) {
        throw std::runtime_error("MCGA function-$06 nested pointer offset mismatch");
    }
    nested_pointer_offset_ = read.value;
    nested_pointer_offset_read_ = read;
    last_sequence_ = read.sequence;
    state_ = MillenniumDosVideoFunctionSixMcgaPointerSetupState::awaiting_nested_pointer_segment;
}

void MillenniumDosVideoFunctionSixMcgaPointerSetupSession::observe_nested_pointer_segment(
    const MillenniumDosVideoFunctionSixMcgaSourceWordRead& read) {
    require_state(MillenniumDosVideoFunctionSixMcgaPointerSetupState::awaiting_nested_pointer_segment,
        read.sequence, read.instruction_address);
    if (first_pointer_offset_ > 0xfff8U || read.ds != first_pointer_segment_
        || read.offset != static_cast<std::uint16_t>(first_pointer_offset_ + 6U)) {
        throw std::runtime_error("MCGA function-$06 nested pointer segment mismatch");
    }
    nested_pointer_segment_ = read.value;
    nested_pointer_segment_read_ = read;
    last_sequence_ = read.sequence;
    state_ = MillenniumDosVideoFunctionSixMcgaPointerSetupState::awaiting_stack_pop;
}

void MillenniumDosVideoFunctionSixMcgaPointerSetupSession::observe_stack_pop(
    const MillenniumDosVideoFunctionSixMcgaStackWordRead& read) {
    require_state(MillenniumDosVideoFunctionSixMcgaPointerSetupState::awaiting_stack_pop,
        read.sequence, read.instruction_address);
    if (read.ss != ss_ || read.offset != stack_offset_) {
        throw std::runtime_error("MCGA function-$06 saved DS pop mismatch");
    }
    last_sequence_ = read.sequence;
    outcome_ = MillenniumDosVideoFunctionSixMcgaPointerSetupOutcome{
        initial_ds_, initial_sp_, descriptor_offset_read_, descriptor_segment_read_,
        source_header_read_, nested_pointer_offset_read_, nested_pointer_segment_read_, read,
        nested_pointer_segment_, nested_pointer_offset_, header_word_, header_word_, read.value};
    state_ = MillenniumDosVideoFunctionSixMcgaPointerSetupState::complete;
}

} // namespace eon
