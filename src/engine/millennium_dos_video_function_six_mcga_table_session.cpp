#include "engine/millennium_dos_video_function_six_mcga_table_session.hpp"

#include "data/sha256.hpp"

#include <stdexcept>

namespace eon {

MillenniumDosVideoFunctionSixMcgaTableSession::MillenniumDosVideoFunctionSixMcgaTableSession(
    const std::span<const std::uint8_t> english_mcga_driver,
    const std::uint16_t descriptor_es,
    const std::uint16_t descriptor_bx,
    const MillenniumDosVideoDataSegment ds)
    : descriptor_es_(descriptor_es),
      descriptor_offset_(static_cast<std::uint16_t>(descriptor_bx + 0x12U)), ds_(ds) {
    constexpr std::size_t offset = 0x071d;
    constexpr std::size_t span_size = 0x10;
    if (english_mcga_driver.size() != 4'366 || offset > english_mcga_driver.size()
        || english_mcga_driver.size() - offset < span_size
        || to_hex(sha256(english_mcga_driver.subspan(offset, span_size)))
            != "de4e68006f0e2cd60399d7ab8821fcb0eb132c7b9a740e517f22629cb3411a7c") {
        throw std::runtime_error("Unsupported English MCGA function-$06 table span");
    }
    static_cast<void>(parse_millennium_dos_video_driver(
        english_mcga_driver, MillenniumDosVideoDriverKind::mcga));
}

void MillenniumDosVideoFunctionSixMcgaTableSession::observe_descriptor_byte_read(
    const MillenniumDosVideoFunctionSixMcgaDescriptorByteRead& read) {
    if (state_ != MillenniumDosVideoFunctionSixMcgaTableState::awaiting_descriptor_byte
        || read.sequence != last_sequence_ + 1 || read.instruction_address != 0x071d
        || read.es != descriptor_es_ || read.offset != descriptor_offset_) {
        throw std::runtime_error("MCGA function-$06 descriptor byte read mismatch");
    }
    last_sequence_ = read.sequence;
    descriptor_read_ = read;
    // MOV DI,$b0 / XOR AH,AH / ADD DI,AX: the table index is a 16-bit
    // zero-extended byte added to the immediate base.
    di_ = static_cast<std::uint16_t>(0x00b0U + read.value);
    table_boundary_ = {last_sequence_ + 1, 0x0728, ds_, di_};
    state_ = MillenniumDosVideoFunctionSixMcgaTableState::awaiting_table_byte;
}

void MillenniumDosVideoFunctionSixMcgaTableSession::observe_table_byte_read(
    const MillenniumDosVideoFunctionSixMcgaTableByteRead& read) {
    if (state_ != MillenniumDosVideoFunctionSixMcgaTableState::awaiting_table_byte
        || read.sequence != table_boundary_.sequence
        || read.instruction_address != table_boundary_.instruction_address
        || read.ds != table_boundary_.ds || read.di != table_boundary_.di) {
        throw std::runtime_error("MCGA function-$06 DS table byte read mismatch");
    }
    last_sequence_ = read.sequence;
    table_read_ = read;
    store_ = {0x072a, ds_, 0x07b9, read.value};
    state_ = MillenniumDosVideoFunctionSixMcgaTableState::complete;
}

} // namespace eon
