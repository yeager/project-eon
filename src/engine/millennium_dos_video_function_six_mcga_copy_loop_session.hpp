#pragma once

#include "engine/native_runtime_memory.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace eon {

struct MillenniumDosVideoFunctionSixMcgaCallerOutcome;

enum class MillenniumDosVideoFunctionSixMcgaCopyLoopKind {
    word_aligned,
    byte_aligned,
};

struct MillenniumDosVideoFunctionSixMcgaCopyLoopRegisters {
    std::uint16_t ax = 0;
    std::uint16_t bx = 0;
    std::uint16_t cx = 0;
    std::uint16_t dx = 0;
    std::uint16_t si = 0;
    std::uint16_t di = 0;
    std::uint16_t bp = 0;
    std::uint16_t ds = 0;
    std::uint16_t es = 0;
    bool direction_flag = false;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaCopyLoopRegisters&) const = default;
};

struct MillenniumDosVideoFunctionSixMcgaCopyPhase {
    std::uint16_t instruction_address = 0;
    std::uint16_t ds = 0;
    std::uint16_t es = 0;
    std::uint16_t source_start = 0;
    std::uint16_t destination_start = 0;
    std::uint16_t element_count = 0;
    std::uint8_t element_size = 0;
    bool direction_flag = false;
    std::uint16_t source_end = 0;
    std::uint16_t destination_end = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaCopyPhase&) const = default;
};

struct MillenniumDosVideoFunctionSixMcgaCopyRow {
    std::uint16_t bp_before = 0;
    std::vector<MillenniumDosVideoFunctionSixMcgaCopyPhase> phases;
    std::uint16_t source_after_row_stride = 0;
    std::uint16_t destination_after_row_stride = 0;
    std::uint16_t bp_after = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaCopyRow&) const = default;
};

struct MillenniumDosVideoFunctionSixMcgaCopyLoopOutcome {
    MillenniumDosVideoFunctionSixMcgaCopyLoopKind kind =
        MillenniumDosVideoFunctionSixMcgaCopyLoopKind::word_aligned;
    MillenniumDosVideoFunctionSixMcgaCopyLoopRegisters initial_registers;
    MillenniumDosVideoFunctionSixMcgaCopyLoopRegisters final_registers;
    std::uint16_t next_instruction = 0;
    std::vector<MillenniumDosVideoFunctionSixMcgaCopyRow> rows;
    constexpr bool operator==(const MillenniumDosVideoFunctionSixMcgaCopyLoopOutcome&) const = default;
};

// Hash-bound 16-bit register/address observation of one MCGA copy loop.
// REP MOVS memory contents, mapped extents, overlap effects and pixel meaning
// are deliberately outside this model. Execution is bounded by max_rows.
class MillenniumDosVideoFunctionSixMcgaCopyLoopSession {
public:
    MillenniumDosVideoFunctionSixMcgaCopyLoopSession(
        std::span<const std::uint8_t> english_mcga_driver,
        MillenniumDosVideoFunctionSixMcgaCopyLoopKind kind,
        MillenniumDosVideoFunctionSixMcgaCopyLoopRegisters initial_registers,
        std::uint32_t max_rows = 4096);

    [[nodiscard]] const MillenniumDosVideoFunctionSixMcgaCopyLoopOutcome& outcome() const {
        return outcome_;
    }

private:
    MillenniumDosVideoFunctionSixMcgaCopyLoopOutcome outcome_;
};

enum class MillenniumDosRealModeAddressMapping {
    a20_wrapped_20_bit,
    unwrapped_21_bit,
};

struct MillenniumDosVideoFunctionSixMcgaCopyMemoryResult {
    bool accepted = false;
    bool memory_applied = false;
    std::string error;
    std::uint64_t byte_write_count = 0;
    std::uint64_t distinct_destination_bytes = 0;
};

// Execute the hash-bound MOVS phases against explicitly initialized guest
// memory. Real-mode bus mapping is an explicit input because the A20 state is
// not implied by the driver's bytes. All reads and ordered overlap effects are
// preflighted before one atomic final-memory batch is committed.
[[nodiscard]] MillenniumDosVideoFunctionSixMcgaCopyMemoryResult
execute_millennium_dos_video_function_six_mcga_copy_loop(
    std::span<const std::uint8_t> english_mcga_driver,
    const MillenniumDosVideoFunctionSixMcgaCopyLoopOutcome& observed_loop,
    MillenniumDosRealModeAddressMapping address_mapping,
    NativeRuntimeMemory& memory,
    std::string batch_id,
    std::uint32_t max_rows = 4096,
    std::uint64_t max_total_bytes = 1'048'576);

[[nodiscard]] MillenniumDosVideoFunctionSixMcgaCopyMemoryResult
execute_millennium_dos_video_function_six_mcga_copy_loop(
    std::span<const std::uint8_t> english_mcga_driver,
    const MillenniumDosVideoFunctionSixMcgaCallerOutcome& completed_caller,
    MillenniumDosRealModeAddressMapping address_mapping,
    NativeRuntimeMemory& memory,
    std::string batch_id,
    std::uint64_t max_total_bytes = 1'048'576);

// Convenience overload for mechanics tests and isolated callers. Integrated
// paths should pass the loop already admitted by the caller continuation.
[[nodiscard]] MillenniumDosVideoFunctionSixMcgaCopyMemoryResult
execute_millennium_dos_video_function_six_mcga_copy_loop(
    std::span<const std::uint8_t> english_mcga_driver,
    MillenniumDosVideoFunctionSixMcgaCopyLoopKind kind,
    MillenniumDosVideoFunctionSixMcgaCopyLoopRegisters initial_registers,
    MillenniumDosRealModeAddressMapping address_mapping,
    NativeRuntimeMemory& memory,
    std::string batch_id,
    std::uint32_t max_rows = 4096,
    std::uint64_t max_total_bytes = 1'048'576);

} // namespace eon
