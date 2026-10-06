#include "engine/millennium_dos_video_function_six_mcga_overlay_loop_session.hpp"

#include "data/millennium_dos_video_driver.hpp"
#include "data/sha256.hpp"

#include <stdexcept>
#include <string_view>
#include <utility>

namespace eon {

MillenniumDosVideoFunctionSixMcgaOverlayLoopSession::MillenniumDosVideoFunctionSixMcgaOverlayLoopSession(
    const std::span<const std::uint8_t> english_mcga_driver,
    const MillenniumDosVideoFunctionSixMcgaOverlayRegisters initial_registers,
    const std::uint32_t max_rows,
    const std::uint32_t max_total_bytes,
    const std::uint64_t preceding_sequence)
    : initial_registers_(initial_registers), registers_(initial_registers),
      max_rows_(max_rows), max_total_bytes_(max_total_bytes), last_sequence_(preceding_sequence) {
    constexpr std::size_t driver_size = 4'366;
    constexpr std::string_view driver_hash =
        "bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228";
    constexpr std::size_t span_offset = 0x07b5;
    constexpr std::size_t span_size = 0x12;
    constexpr std::string_view span_hash =
        "1f74b1e9894303e3e6772b8c95a687554f7b989175ea27e756f24293d32fcded";
    if (english_mcga_driver.size() != driver_size
        || to_hex(sha256(english_mcga_driver)) != driver_hash
        || to_hex(sha256(english_mcga_driver.subspan(span_offset, span_size))) != span_hash) {
        throw std::runtime_error("Unsupported English MCGA function-six overlay loop");
    }
    static_cast<void>(parse_millennium_dos_video_driver(
        english_mcga_driver, MillenniumDosVideoDriverKind::mcga));

    constexpr std::uint32_t hard_row_bound = 4096;
    constexpr std::uint32_t hard_total_byte_bound = 1'048'576;
    if (max_rows == 0 || max_rows > hard_row_bound
        || max_total_bytes == 0 || max_total_bytes > hard_total_byte_bound) {
        throw std::runtime_error("MCGA overlay-loop bound is outside the supported limit");
    }

    rows_required_ = initial_registers.bp == 0
        ? 65'536U : static_cast<std::uint32_t>(initial_registers.bp);
    if (rows_required_ > max_rows_) {
        throw std::runtime_error("MCGA overlay-loop execution exceeds the explicit row bound");
    }

    // XCHG AX,CX is at the sole entry point $07b5. The outer JNZ at $07c4
    // targets $07b6, so subsequent rows do not repeat this exchange.
    std::swap(registers_.ax, registers_.cx);
    rows_.reserve(rows_required_);
    stack_writes_.reserve(rows_required_);
    prepare_row();
}

void MillenniumDosVideoFunctionSixMcgaOverlayLoopSession::require_sequence(
    const std::uint64_t sequence) const {
    if (sequence != last_sequence_ + 1) {
        throw std::runtime_error("MCGA overlay-loop observation sequence mismatch");
    }
}

void MillenniumDosVideoFunctionSixMcgaOverlayLoopSession::prepare_row() {
    pushed_cx_ = registers_.cx;
    registers_.sp = static_cast<std::uint16_t>(registers_.sp - 2U);
    const auto remaining_bytes = pushed_cx_ == 0
        ? 65'536U : static_cast<std::uint32_t>(pushed_cx_);
    if (remaining_bytes > max_total_bytes_ - bytes_observed_) {
        throw std::runtime_error("MCGA overlay-loop execution exceeds the explicit byte bound");
    }
    row_bytes_remaining_ = remaining_bytes;
    current_row_source_start_ = registers_.si;
    current_row_destination_start_ = registers_.di;
    current_row_effects_.clear();
    current_row_effects_.reserve(remaining_bytes);
    state_ = MillenniumDosVideoFunctionSixMcgaOverlayState::awaiting_stack_push;
}

void MillenniumDosVideoFunctionSixMcgaOverlayLoopSession::observe_stack_write(
    const MillenniumDosVideoFunctionSixMcgaOverlayStackWrite& write) {
    if (state_ != MillenniumDosVideoFunctionSixMcgaOverlayState::awaiting_stack_push
        || write.instruction_address != 0x07b6 || write.ss != registers_.ss
        || write.offset != registers_.sp || write.value != pushed_cx_) {
        throw std::runtime_error("MCGA overlay-loop PUSH CX observation mismatch");
    }
    require_sequence(write.sequence);
    last_sequence_ = write.sequence;
    stack_writes_.push_back(write);
    state_ = MillenniumDosVideoFunctionSixMcgaOverlayState::awaiting_source_read;
}

void MillenniumDosVideoFunctionSixMcgaOverlayLoopSession::observe_source_read(
    const MillenniumDosVideoFunctionSixMcgaOverlaySourceRead& read) {
    if (state_ != MillenniumDosVideoFunctionSixMcgaOverlayState::awaiting_source_read
        || read.instruction_address != 0x07b7 || read.ds != registers_.ds
        || read.offset != registers_.si) {
        throw std::runtime_error("MCGA overlay-loop LODSB observation mismatch");
    }
    require_sequence(read.sequence);
    last_sequence_ = read.sequence;
    source_offset_ = read.offset;
    source_value_ = read.value;
    registers_.ax = static_cast<std::uint16_t>((registers_.ax & 0xff00U) | read.value);
    registers_.si = static_cast<std::uint16_t>(registers_.si
        + (registers_.direction_flag ? 0xffffU : 1U));
    state_ = MillenniumDosVideoFunctionSixMcgaOverlayState::awaiting_destination_read;
}

void MillenniumDosVideoFunctionSixMcgaOverlayLoopSession::observe_destination_read(
    const MillenniumDosVideoFunctionSixMcgaOverlayDestinationRead& read) {
    if (state_ != MillenniumDosVideoFunctionSixMcgaOverlayState::awaiting_destination_read
        || read.instruction_address != 0x07b8 || read.es != registers_.es
        || read.offset != registers_.di) {
        throw std::runtime_error("MCGA overlay-loop OR destination read mismatch");
    }
    require_sequence(read.sequence);
    last_sequence_ = read.sequence;
    destination_offset_ = read.offset;
    destination_before_ = read.value;
    state_ = MillenniumDosVideoFunctionSixMcgaOverlayState::awaiting_destination_write;
}

void MillenniumDosVideoFunctionSixMcgaOverlayLoopSession::observe_destination_write(
    const MillenniumDosVideoFunctionSixMcgaOverlayDestinationWrite& write) {
    const auto expected_value = static_cast<std::uint8_t>(destination_before_ | source_value_);
    if (state_ != MillenniumDosVideoFunctionSixMcgaOverlayState::awaiting_destination_write
        || write.instruction_address != 0x07b8 || write.es != registers_.es
        || write.offset != destination_offset_ || write.value != expected_value) {
        throw std::runtime_error("MCGA overlay-loop OR destination write mismatch");
    }
    require_sequence(write.sequence);
    last_sequence_ = write.sequence;
    current_row_effects_.push_back({source_offset_, destination_offset_, source_value_,
        destination_before_, expected_value});
    ++bytes_observed_;

    // The following INC DI is unconditional; only LODSB honors the direction
    // flag. LOOP decrements CX and ends the row only when the wrapped value is 0.
    registers_.di = static_cast<std::uint16_t>(registers_.di + 1U);
    registers_.cx = static_cast<std::uint16_t>(registers_.cx - 1U);
    --row_bytes_remaining_;
    if (row_bytes_remaining_ == 0) {
        registers_.di = static_cast<std::uint16_t>(registers_.di + registers_.dx);
        registers_.si = static_cast<std::uint16_t>(registers_.si + registers_.bx);
        state_ = MillenniumDosVideoFunctionSixMcgaOverlayState::awaiting_stack_pop;
    } else {
        state_ = MillenniumDosVideoFunctionSixMcgaOverlayState::awaiting_source_read;
    }
}

void MillenniumDosVideoFunctionSixMcgaOverlayLoopSession::observe_stack_read(
    const MillenniumDosVideoFunctionSixMcgaOverlayStackRead& read) {
    if (state_ != MillenniumDosVideoFunctionSixMcgaOverlayState::awaiting_stack_pop
        || read.instruction_address != 0x07c2 || read.ss != registers_.ss
        || read.offset != registers_.sp) {
        throw std::runtime_error("MCGA overlay-loop POP CX observation mismatch");
    }
    require_sequence(read.sequence);
    last_sequence_ = read.sequence;
    registers_.cx = read.value;
    registers_.sp = static_cast<std::uint16_t>(registers_.sp + 2U);
    registers_.bp = static_cast<std::uint16_t>(registers_.bp - 1U);
    rows_.push_back({current_row_source_start_, current_row_destination_start_,
        static_cast<std::uint32_t>(current_row_effects_.size()), registers_.si,
        registers_.di, read.value, registers_.bp, std::move(current_row_effects_)});

    if (rows_.size() == rows_required_) {
        outcome_ = MillenniumDosVideoFunctionSixMcgaOverlayOutcome{
            initial_registers_, registers_, 0x07c6, std::move(rows_)};
        state_ = MillenniumDosVideoFunctionSixMcgaOverlayState::complete;
        return;
    }
    prepare_row();
}

} // namespace eon
