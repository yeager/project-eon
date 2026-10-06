#include "engine/millennium_dos_video_function_six_mcga_copy_loop_session.hpp"

#include "data/millennium_dos_video_driver.hpp"
#include "data/sha256.hpp"

#include <stdexcept>
#include <string_view>
#include <utility>

namespace eon {
namespace {

std::uint16_t step_offset(const std::uint16_t offset, const std::uint32_t elements,
    const std::uint8_t element_size, const bool backwards) {
    const auto amount = static_cast<std::uint16_t>(elements * element_size);
    return static_cast<std::uint16_t>(backwards ? offset - amount : offset + amount);
}

MillenniumDosVideoFunctionSixMcgaCopyPhase phase(
    const std::uint16_t address,
    const MillenniumDosVideoFunctionSixMcgaCopyLoopRegisters& registers,
    const std::uint16_t count, const std::uint8_t element_size) {
    const auto backwards = registers.direction_flag;
    return {address, registers.ds, registers.es, registers.si, registers.di,
        count, element_size, backwards,
        step_offset(registers.si, count, element_size, backwards),
        step_offset(registers.di, count, element_size, backwards)};
}

} // namespace

MillenniumDosVideoFunctionSixMcgaCopyLoopSession::MillenniumDosVideoFunctionSixMcgaCopyLoopSession(
    const std::span<const std::uint8_t> english_mcga_driver,
    const MillenniumDosVideoFunctionSixMcgaCopyLoopKind kind,
    const MillenniumDosVideoFunctionSixMcgaCopyLoopRegisters initial_registers,
    const std::uint32_t max_rows) {
    constexpr std::size_t driver_size = 4'366;
    constexpr std::string_view driver_hash =
        "bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228";
    if (english_mcga_driver.size() != driver_size
        || to_hex(sha256(english_mcga_driver)) != driver_hash) {
        throw std::runtime_error("Unsupported English MCGA driver for copy-loop observation");
    }
    static_cast<void>(parse_millennium_dos_video_driver(
        english_mcga_driver, MillenniumDosVideoDriverKind::mcga));

    constexpr std::uint32_t hard_row_bound = 4096;
    if (max_rows == 0 || max_rows > hard_row_bound) {
        throw std::runtime_error("MCGA copy-loop row bound is outside the supported limit");
    }
    const auto required_rows = initial_registers.bp == 0
        ? 65'536U : static_cast<std::uint32_t>(initial_registers.bp);
    if (required_rows > max_rows) {
        throw std::runtime_error("MCGA copy-loop execution exceeds the explicit row bound");
    }

    const auto is_word_aligned = kind == MillenniumDosVideoFunctionSixMcgaCopyLoopKind::word_aligned;
    if (!is_word_aligned && kind != MillenniumDosVideoFunctionSixMcgaCopyLoopKind::byte_aligned) {
        throw std::runtime_error("Unknown MCGA function-six copy-loop kind");
    }
    constexpr std::size_t word_offset = 0x078b;
    constexpr std::size_t word_size = 0x12;
    constexpr std::string_view word_hash =
        "9f2573a0ea405375df0169035c80a9fca97f27720fb5fb51a8f7cb9f5d45a3bc";
    constexpr std::size_t byte_offset = 0x07a1;
    constexpr std::size_t byte_size = 0x14;
    constexpr std::string_view byte_hash =
        "b4fe25daa057cf4c4b8d1b31e81956dd822dd1bf5fe3cde4b9f464cabd9f4146";
    const auto offset = is_word_aligned ? word_offset : byte_offset;
    const auto size = is_word_aligned ? word_size : byte_size;
    const auto expected_hash = is_word_aligned ? word_hash : byte_hash;
    if (english_mcga_driver.size() - offset < size
        || to_hex(sha256(english_mcga_driver.subspan(offset, size))) != expected_hash) {
        throw std::runtime_error("Unsupported MCGA function-six copy-loop span");
    }

    outcome_.kind = kind;
    outcome_.initial_registers = initial_registers;
    auto registers = initial_registers;
    outcome_.rows.reserve(required_rows);

    for (std::uint32_t row_index = 0; row_index < required_rows; ++row_index) {
        MillenniumDosVideoFunctionSixMcgaCopyRow row;
        row.bp_before = registers.bp;
        if (is_word_aligned) {
            const auto words = static_cast<std::uint16_t>(registers.ax >> 1U);
            const auto tail = static_cast<std::uint16_t>(registers.ax & 1U);
            row.phases.push_back(phase(0x078f, registers, words, 2));
            registers.si = row.phases.back().source_end;
            registers.di = row.phases.back().destination_end;
            row.phases.push_back(phase(0x0793, registers, tail, 1));
            registers.si = row.phases.back().source_end;
            registers.di = row.phases.back().destination_end;
        } else {
            // The initial MOVSB is unconditional, including AX=0. The
            // following DEC CX therefore makes the zero case a 65,536-byte
            // 16-bit count, exactly as the instructions encode.
            row.phases.push_back(phase(0x07a3, registers, 1, 1));
            registers.si = row.phases.back().source_end;
            registers.di = row.phases.back().destination_end;
            const auto remaining = static_cast<std::uint16_t>(registers.ax - 1U);
            const auto words = static_cast<std::uint16_t>(remaining >> 1U);
            const auto tail = static_cast<std::uint16_t>(remaining & 1U);
            row.phases.push_back(phase(0x07a7, registers, words, 2));
            registers.si = row.phases.back().source_end;
            registers.di = row.phases.back().destination_end;
            row.phases.push_back(phase(0x07ab, registers, tail, 1));
            registers.si = row.phases.back().source_end;
            registers.di = row.phases.back().destination_end;
        }

        // Each final REP MOVSB exhausts CX, including its zero-count case.
        registers.cx = 0;

        // ADD DI,DX; ADD SI,BX; DEC BP; JNZ loop-head.
        registers.di = static_cast<std::uint16_t>(registers.di + registers.dx);
        registers.si = static_cast<std::uint16_t>(registers.si + registers.bx);
        registers.bp = static_cast<std::uint16_t>(registers.bp - 1U);
        row.source_after_row_stride = registers.si;
        row.destination_after_row_stride = registers.di;
        row.bp_after = registers.bp;
        outcome_.rows.push_back(std::move(row));
    }
    outcome_.final_registers = registers;
    outcome_.next_instruction = is_word_aligned ? 0x079c : 0x07b4;
}

} // namespace eon
