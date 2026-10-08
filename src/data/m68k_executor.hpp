#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace eon::m68k {

struct MachineState {
    std::uint32_t pc = 0;
    std::uint16_t sr = 0;
    std::array<std::uint32_t, 8> data{};
    std::array<std::uint32_t, 8> address{};
};

struct MemoryRange {
    std::uint32_t base = 0;
    std::span<std::uint8_t> bytes;
    bool writable = false;
};

struct BranchCheckpoint {
    std::uint32_t instruction_address = 0;
    std::uint16_t status_register = 0;
    std::uint32_t target_address = 0;
    bool taken = false;
};

enum class StopReason {
    requested_address,
    unsupported_instruction,
    trap_instruction,
    memory_fault,
    instruction_limit,
};

struct ExecutionResult {
    MachineState state;
    StopReason reason = StopReason::instruction_limit;
    std::uint64_t instructions_executed = 0;
    std::uint16_t stop_opcode = 0;
    std::vector<BranchCheckpoint> branches;
};

// Executes only instructions used by hash-verified Atari loader and bounded
// Deuteros prefixes. Instruction fetches are bounded to `code`; data accesses
// must fit wholly inside one supplied memory range. No OS or hardware service
// is emulated. Explicitly observed hardware bytes may be supplied as scratch
// memory ranges by the caller.
[[nodiscard]] ExecutionResult execute(std::span<const std::uint8_t> code,
    std::uint32_t code_base, MachineState initial,
    std::span<MemoryRange> memory, std::uint64_t max_steps,
    std::uint32_t stop_address);

[[nodiscard]] ExecutionResult execute(std::span<const std::uint8_t> code,
    std::uint32_t code_base, MachineState initial,
    std::span<MemoryRange> memory, std::uint64_t max_steps,
    std::span<const std::uint32_t> stop_addresses);

} // namespace eon::m68k
